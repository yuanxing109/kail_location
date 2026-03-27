#include "config.h"

#include <android/log.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define LOG_TAG "GaitConfig"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace gait {

static constexpr int64_t kReloadIntervalNs = 1000000000LL;

static bool ParseMode(const char* s, Mode* out) {
  if (!s || !out) return false;
  if (strcmp(s, "walk") == 0) { *out = Mode::Walk; return true; }
  if (strcmp(s, "run") == 0) { *out = Mode::Run; return true; }
  if (strcmp(s, "fast_run") == 0) { *out = Mode::FastRun; return true; }
  return false;
}

Config::Config(const char* path) : path_(path) {
  last_params_.steps_per_minute = 120.0f;
  last_params_.mode = Mode::Walk;
  last_params_.enable = true;
}

bool Config::MaybeReload(int64_t now_ns, Params* out) {
  if (!out) return false;
  if (now_ns - last_check_ns_ < kReloadIntervalNs) {
    if (has_last_) *out = last_params_;
    return false;
  }
  last_check_ns_ = now_ns;

  Params p{};
  if (!ReadFile(&p)) {
    if (has_last_) *out = last_params_;
    return false;
  }

  last_params_ = p;
  has_last_ = true;
  *out = p;
  return true;
}

bool Config::ReadFile(Params* out) {
  FILE* fp = std::fopen(path_, "re");
  if (!fp) {
    return false;
  }

  Params p{};
  p.steps_per_minute = 120.0f;
  p.mode = Mode::Walk;
  p.enable = true;

  char line[256];
  while (std::fgets(line, sizeof(line), fp)) {
    char* nl = std::strchr(line, '\n');
    if (nl) *nl = '\0';

    if (line[0] == '\0') continue;

    char key[64]{};
    char val[128]{};
    if (std::sscanf(line, "%63[^=]=%127s", key, val) != 2) continue;

    if (std::strcmp(key, "steps_per_minute") == 0) {
      float spm = std::strtof(val, nullptr);
      if (spm > 0.0f) p.steps_per_minute = spm;
    } else if (std::strcmp(key, "mode") == 0) {
      Mode m;
      if (ParseMode(val, &m)) p.mode = m;
    } else if (std::strcmp(key, "enable") == 0) {
      int en = std::atoi(val);
      p.enable = (en != 0);
    }
  }

  std::fclose(fp);

  *out = p;
  ALOGI("Loaded config: enable=%d mode=%d spm=%.2f",
        p.enable ? 1 : 0, static_cast<int>(p.mode), p.steps_per_minute);
  return true;
}

}  // namespace gait