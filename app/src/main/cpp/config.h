#pragma once

#include "simulate.h"

#include <cstdint>

namespace gait {

class Config {
public:
  explicit Config(const char* path);

  bool MaybeReload(int64_t now_ns, Params* out);

private:
  bool ReadFile(Params* out);

private:
  const char* path_;
  int64_t last_check_ns_ = 0;
  Params last_params_{};
  bool has_last_ = false;
};

}  // namespace gait