#include <jni.h>

#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <dobby.h>

#include "sensor_simulator.h"

#define LOG_TAG "NativeHook"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Symbol names - multiple variations to try
static const char* kThreadLoopSymbols[] = {
    "_ZN7android13SensorService10threadLoopEv",
    "_ZN7android13SensorService9threadLoopEv", 
    "android::SensorService::threadLoop",
    nullptr
};

// Original function pointer type
typedef bool (*ThreadLoopFunc)(void*);

// Global state
static ThreadLoopFunc original_thread_loop = nullptr;
static bool hook_installed = false;
static bool initialized = false;

// Forward declaration
extern "C" bool hooked_threadLoop(void* this_ptr);

// ============================================================================
// Symbol Resolution via /proc/self/maps
// ============================================================================

static void scan_maps_for_sensor_libs() {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        ALOGE("Failed to open /proc/self/maps");
        return;
    }
    
    char line[512];
    ALOGI("=== Scanning /proc/self/maps for sensor libs ===");
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "sensor") || strstr(line, "android.hardware")) {
            ALOGI("MAP: %s", line);
        }
    }
    
    ALOGI("=== End of maps scan ===");
    fclose(fp);
}

static void* get_lib_base(const char* lib_name) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        ALOGE("Failed to open /proc/self/maps");
        return nullptr;
    }
    
    char line[512];
    void* base_addr = nullptr;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, lib_name) && strstr(line, "r-xp")) {
            ALOGI("Found in maps: %s", line);
            // Parse base address
            uint64_t start;
            sscanf(line, "%lx-", &start);
            base_addr = reinterpret_cast<void*>(start);
            ALOGI("Base address of %s: %p", lib_name, base_addr);
            break;
        }
    }
    
    fclose(fp);
    return base_addr;
}

// ELF structures
struct Elf64_Ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct Elf64_Dyn {
    int64_t d_tag;
    union {
        uint64_t d_val;
        void* d_ptr;
    } d_un;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_STRSZ 10
#define DT_SYMENT 11

static void* find_symbol_in_elf(void* base_addr, const char* symbol_name) {
    // Read ELF header
    Elf64_Ehdr* ehdr = reinterpret_cast<Elf64_Ehdr*>(base_addr);
    
    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' || 
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        ALOGE("Invalid ELF header at %p", base_addr);
        return nullptr;
    }
    
    ALOGI("ELF header valid at %p", base_addr);
    
    // Find dynamic section
    uint64_t dyn_offset = 0;
    Elf64_Phdr* phdr = reinterpret_cast<Elf64_Phdr*>(reinterpret_cast<char*>(base_addr) + ehdr->e_phoff);
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == 2) { // PT_DYNAMIC
            dyn_offset = phdr[i].p_offset;
            ALOGI("Found PT_DYNAMIC at offset 0x%lx", dyn_offset);
            break;
        }
    }
    
    if (!dyn_offset) {
        ALOGE("No PT_DYNAMIC found");
        return nullptr;
    }
    
    // Parse dynamic section to find SYMTAB and STRTAB
    char* strtab = nullptr;
    Elf64_Sym* symtab = nullptr;
    size_t sym_count = 0;
    
    Elf64_Dyn* dyn = reinterpret_cast<Elf64_Dyn*>(reinterpret_cast<char*>(base_addr) + dyn_offset);
    for (int i = 0; dyn[i].d_tag != 0; i++) {
        if (dyn[i].d_tag == DT_STRTAB) {
            strtab = reinterpret_cast<char*>(dyn[i].d_un.d_ptr);
            ALOGI("Found STRTAB at %p", strtab);
        } else if (dyn[i].d_tag == DT_SYMTAB) {
            symtab = reinterpret_cast<Elf64_Sym*>(dyn[i].d_un.d_ptr);
            ALOGI("Found SYMTAB at %p", symtab);
        } else if (dyn[i].d_tag == DT_SYMENT) {
            // Should be sizeof(Elf64_Sym) = 24
        } else if (dyn[i].d_tag == DT_STRSZ) {
            // String table size
        }
    }
    
    if (!strtab || !symtab) {
        ALOGE("Could not find strtab or symtab");
        return nullptr;
    }
    
    // Search for symbol - iterate through symtab
    // Note: We don't know exact size, so we'll search for a reasonable range
    ALOGI("Searching for symbol: %s", symbol_name);
    
    // Try to find symbol by iterating (this is a simplified approach)
    // In reality we'd need the symtab size from elsewhere
    for (int i = 0; i < 5000; i++) {
        Elf64_Sym* sym = &symtab[i];
        if (sym->st_name == 0) continue;
        
        const char* sym_name = strtab + sym->st_name;
        if (sym_name && strstr(sym_name, symbol_name)) {
            ALOGI("Found symbol: %s at index %d, value: %p", sym_name, i, 
                  reinterpret_cast<void*>(sym->st_value));
            
            if (sym->st_value != 0) {
                return reinterpret_cast<void*>(sym->st_value);
            }
        }
    }
    
    ALOGE("Symbol %s not found in ELF", symbol_name);
    return nullptr;
}

static void* resolve_symbol(const char* lib, const char* symbol) {
    ALOGI("Resolving symbol: %s", symbol);
    
    // Try each symbol name variation
    for (int i = 0; kThreadLoopSymbols[i] != nullptr; i++) {
        const char* sym_name = kThreadLoopSymbols[i];
        
        // Method 1: Try RTLD_DEFAULT
        void* sym = dlsym(RTLD_DEFAULT, sym_name);
        if (sym) {
            ALOGI("Found %s via RTLD_DEFAULT -> %p", sym_name, sym);
            return sym;
        }
        
        // Method 2: Try RTLD_NEXT
        sym = dlsym(RTLD_NEXT, sym_name);
        if (sym) {
            ALOGI("Found %s via RTLD_NEXT -> %p", sym_name, sym);
            return sym;
        }
        
        ALOGI("Symbol %s not found via dlsym", sym_name);
    }
    
    // Method 3: Parse ELF directly to find symbol
    const char* lib_name = "libsensorservice.so";
    void* base_addr = get_lib_base(lib_name);
    
    if (base_addr) {
        ALOGI("Found %s at base %p, parsing ELF for symbol", lib_name, base_addr);
        
        // Try each symbol variation with ELF parser
        for (int i = 0; kThreadLoopSymbols[i] != nullptr; i++) {
            const char* sym_name = kThreadLoopSymbols[i];
            ALOGI("Trying to find %s via ELF parsing", sym_name);
            
            void* resolved = find_symbol_in_elf(base_addr, sym_name);
            if (resolved) {
                ALOGI("Found %s via ELF -> %p", sym_name, resolved);
                return resolved;
            }
        }
    } else {
        ALOGE("Could not find %s in /proc/self/maps", lib_name);
    }
    
    ALOGE("Failed to resolve symbol: %s", symbol);
    return nullptr;
}

// ============================================================================
// mSensorEventBuffer Access
// ============================================================================

// SensorEvent structure size (typical)
// struct sensors_event_t { int32_t version; int32_t sensor; int32_t type; int64_t timestamp; float data[16]; };
// Size = 4 + 4 + 4 + 8 + 64 = 84 bytes, aligned to 88 bytes

static constexpr size_t kSensorEventSize = 88;
static constexpr size_t kMaxEvents = 256;

// Try to find mSensorEventBuffer offset in SensorService
// This is a heuristic - actual offset may vary by Android version
struct SensorServiceOffsets {
    uint32_t sensor_event_buffer;  // mSensorEventBuffer offset
    uint32_t event_count;         // mSensorEventCount offset
};

// Known offsets for different Android versions
// These are estimated values and may need adjustment
static const std::vector<SensorServiceOffsets> kOffsetCandidates = {
    {0x1B0, 0x1B8},  // Android 13
    {0x1C0, 0x1C8},  // Android 14
    {0x1D0, 0x1D8},  // Android 15
};

static sensors_event_t* get_sensor_buffer(void* this_ptr) {
    if (!this_ptr) return nullptr;
    
    // Try each known offset
    for (const auto& offsets : kOffsetCandidates) {
        auto* buffer = reinterpret_cast<sensors_event_t*>(
            reinterpret_cast<char*>(this_ptr) + offsets.sensor_event_buffer);
        
        // Sanity check - buffer should be readable
        if (buffer) {
            ALOGD("Found sensor buffer at offset 0x%x -> %p", 
                  offsets.sensor_event_buffer, buffer);
            return buffer;
        }
    }
    
    // Fallback: try common offset
    static constexpr uint32_t kCommonOffset = 0x1B0;
    return reinterpret_cast<sensors_event_t*>(
        reinterpret_cast<char*>(this_ptr) + kCommonOffset);
}

static size_t get_event_count(void* this_ptr) {
    if (!this_ptr) return 0;
    
    // Try known offsets
    for (const auto& offsets : kOffsetCandidates) {
        auto* count = reinterpret_cast<size_t*>(
            reinterpret_cast<char*>(this_ptr) + offsets.event_count);
        if (count && *count > 0 && *count <= kMaxEvents) {
            ALOGD("Found event count at offset 0x%x: %zu", 
                  offsets.event_count, *count);
            return *count;
        }
    }
    
    // Default to reasonable count
    return kMaxEvents;
}

// ============================================================================
// Hook Function
// ============================================================================

extern "C" bool hooked_threadLoop(void* this_ptr) {
    // Call original function first
    bool result = false;
    if (original_thread_loop) {
        result = original_thread_loop(this_ptr);
    }
    
    // Try to get and modify sensor events
    sensors_event_t* buffer = get_sensor_buffer(this_ptr);
    size_t count = get_event_count(this_ptr);
    
    if (buffer && count > 0 && count <= kMaxEvents) {
        // Process sensor events with our simulator
        gait::SensorSimulator::Get().ProcessSensorEvents(buffer, count);
        
        ALOGD("Processed %zu sensor events", count);
    }
    
    return result;
}

// ============================================================================
// JNI Interface for Parameter Setting
// ============================================================================

extern "C" {

// Called from Kotlin via JNI
JNIEXPORT void JNICALL 
Java_com_kail_location_xposed_FakeLocState_nativeSetGaitParams(
    JNIEnv* env, 
    jclass clazz, 
    jfloat spm, 
    jint mode, 
    jboolean enable
) {
    ALOGI("JNI: Set gait params spm=%.2f, mode=%d, enable=%d", spm, mode, enable ? 1 : 0);
    gait::SensorSimulator::Get().UpdateParams(spm, mode, enable);
}

JNIEXPORT jboolean JNICALL 
Java_com_kail_location_xposed_FakeLocState_nativeReloadConfig(
    JNIEnv* env, 
    jclass clazz
) {
    ALOGI("JNI: Reload config");
    return gait::SensorSimulator::Get().ReloadConfig() ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"

// ============================================================================
// Hook Installation
// ============================================================================

static bool install_hook() {
    ALOGI("========================================");
    ALOGI("Installing SensorService::threadLoop hook...");
    ALOGI("========================================");
    
    // Try each symbol variation
    void* thread_loop_addr = nullptr;
    const char* found_symbol = nullptr;
    
    for (int i = 0; kThreadLoopSymbols[i] != nullptr; i++) {
        const char* sym_name = kThreadLoopSymbols[i];
        ALOGI("Trying symbol: %s", sym_name);
        
        thread_loop_addr = resolve_symbol(nullptr, sym_name);
        if (thread_loop_addr) {
            found_symbol = sym_name;
            break;
        }
    }
    
    if (!thread_loop_addr) {
        ALOGE("Failed to resolve threadLoop symbol!");
        ALOGE("Hook installation ABORTED");
        return false;
    }
    
    ALOGI("Found %s at %p", found_symbol, thread_loop_addr);
    
    original_thread_loop = reinterpret_cast<ThreadLoopFunc>(thread_loop_addr);
    
    // Install Dobby hook
    ALOGI("Calling DobbyHook...");
    int ret = DobbyHook(
        thread_loop_addr,
        reinterpret_cast<void*>(&hooked_threadLoop),
        reinterpret_cast<void**>(&original_thread_loop)
    );
    
    if (ret != 0) {
        ALOGE("DobbyHook failed: %d", ret);
        ALOGE("Hook installation FAILED");
        return false;
    }
    
    ALOGI("DobbyHook installed successfully!");
    ALOGI("========================================");
    hook_installed = true;
    return true;
}

// ============================================================================
// Constructor - Auto Initialize
// ============================================================================

__attribute__((constructor))
static void native_hook_init() {
    ALOGI("========================================");
    ALOGI("Native Hook Library Loading (constructor)...");
    ALOGI("========================================");
    
    // Scan maps first to see what's loaded
    scan_maps_for_sensor_libs();
    
    // Initialize sensor simulator
    ALOGI("Initializing SensorSimulator...");
    gait::SensorSimulator::Get().Init();
    ALOGI("SensorSimulator initialized");
    
    // Try to install hook
    ALOGI("Calling install_hook...");
    if (install_hook()) {
        ALOGI("*** Hook installation SUCCESS! ***");
    } else {
        ALOGE("*** Hook installation FAILED! ***");
        ALOGE("Sensor simulation will run in standalone mode");
    }
    
    // Try to reload config from file
    ALOGI("Reloading config from file...");
    bool config_reloaded = gait::SensorSimulator::Get().ReloadConfig();
    ALOGI("Config reload result: %d", config_reloaded ? 1 : 0);
    
    ALOGI("Native Hook Library Loaded!");
    ALOGI("========================================");
}

// ============================================================================
// Destructor
// ============================================================================

__attribute__((destructor))
static void native_hook_destroy() {
    ALOGI("Native Hook Library Unloading...");
    if (hook_installed && original_thread_loop) {
        DobbyDestroy((void*)original_thread_loop);
        ALOGI("Hook destroyed");
    }
}
