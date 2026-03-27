#include "simulate.h"
#include "config.h"
#include <jni.h>
#include <android/log.h>
#include <cstring>
#include <memory>

#define LOG_TAG "GaitJNI"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::unique_ptr<gait::Simulator> g_simulator;
std::unique_ptr<gait::Config> g_config;

}  // namespace

extern "C" {

JNIEXPORT jint JNICALL
Java_com_kail_location_utils_GaitSimulator_nativeInit(JNIEnv* env, jclass clazz, jstring config_path) {
    try {
        ALOGI("nativeInit called");
        const char* path = env->GetStringUTFChars(config_path, nullptr);
        ALOGI("Config path: %s", path);
        g_config = std::make_unique<gait::Config>(path);
        g_simulator = std::make_unique<gait::Simulator>();
        env->ReleaseStringUTFChars(config_path, path);

        ALOGI("GaitSimulator initialized successfully");
        return 0;
    } catch (const std::exception& e) {
        ALOGE("Failed to initialize GaitSimulator: %s", e.what());
        return -1;
    }
}

JNIEXPORT void JNICALL
Java_com_kail_location_utils_GaitSimulator_nativeUpdateParams(JNIEnv* env, jclass clazz,
        jfloat steps_per_minute, jint mode, jboolean enable) {
    if (!g_simulator) {
        ALOGE("Simulator not initialized when updating params");
        return;
    }

    ALOGI("Updating params: spm=%.2f, mode=%d, enable=%d", steps_per_minute, mode, enable ? 1 : 0);
    gait::Params params;
    params.steps_per_minute = steps_per_minute;
    params.mode = static_cast<gait::Mode>(mode);
    params.enable = enable;

    g_simulator->UpdateParams(params);
}

JNIEXPORT void JNICALL
Java_com_kail_location_utils_GaitSimulator_nativeProcessEvents(JNIEnv* env, jclass clazz,
        jlong timestamp_ns, jfloatArray data, jlong type) {
    if (!g_simulator) {
        ALOGE("Simulator not initialized when processing events");
        return;
    }

    // Create a sensors_event_t from the Java data
    sensors_event_t event{};
    event.version = 1;
    event.sensor = 0;
    event.type = static_cast<int>(type);
    event.timestamp = timestamp_ns;

    // Copy data from Java array
    jfloat* java_data = env->GetFloatArrayElements(data, nullptr);
    std::memcpy(event.data, java_data, sizeof(float) * 16);
    env->ReleaseFloatArrayElements(data, java_data, JNI_ABORT);

    // Process the event
    g_simulator->ProcessEvents(&event, 1);

    // Copy modified data back to Java array
    java_data = env->GetFloatArrayElements(data, nullptr);
    std::memcpy(java_data, event.data, sizeof(float) * 16);
    env->ReleaseFloatArrayElements(data, java_data, 0);
}

JNIEXPORT jboolean JNICALL
Java_com_kail_location_utils_GaitSimulator_nativeReloadConfig(JNIEnv* env, jclass clazz, jlong now_ns) {
    if (!g_config || !g_simulator) {
        ALOGE("Config or simulator not initialized when reloading config");
        return JNI_FALSE;
    }

    ALOGI("Reloading config");
    gait::Params params;
    if (g_config->MaybeReload(now_ns, &params)) {
        g_simulator->UpdateParams(params);
        ALOGI("Config reloaded successfully");
        return JNI_TRUE;
    }
    ALOGI("Config not changed");
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_kail_location_utils_GaitSimulator_nativeDestroy(JNIEnv* env, jclass clazz) {
    ALOGI("Destroying GaitSimulator");
    g_simulator.reset();
    g_config.reset();
    ALOGI("GaitSimulator destroyed");
}

}  // extern "C"