#pragma once

#include <cstddef>
#include <cstdint>

#ifndef TYPE_ACCELEROMETER
#define TYPE_ACCELEROMETER 1
#endif
#ifndef TYPE_STEP_DETECTOR
#define TYPE_STEP_DETECTOR 18
#endif
#ifndef TYPE_STEP_COUNTER
#define TYPE_STEP_COUNTER 19
#endif

typedef struct sensors_event_t {
  int version;
  int sensor;
  int type;
  int64_t timestamp;
  float data[16];
} sensors_event_t;

namespace gait {

enum class Mode : int {
  Walk = 0,
  Run = 1,
  FastRun = 2,
};

struct Params {
  float steps_per_minute = 120.0f;
  Mode mode = Mode::Walk;
  bool enable = true;
};

struct State {
  bool initialized = false;

  double phase = 0.0;
  int64_t last_ts_ns = 0;

  double step_counter = 0.0;
  double step_phase_acc = 0.0;

  float current_spm = 120.0f;
  float target_spm = 120.0f;

  uint64_t rng_state = 0x243F6A8885A308D3ULL;
  int64_t last_config_poll_ns = 0;
};

class Simulator {
public:
  Simulator();

  void UpdateParams(const Params& p);
  const Params& GetParams() const;

  void ProcessEvents(sensors_event_t* events, std::size_t count);

private:
  double NextUniform01();
  double NextSignedNoise(double amplitude);

  void EnsureInitialized(int64_t ts_ns);
  void SmoothStepRate(double dt);
  void AdvancePhase(double dt);

  void ApplyAccelerometer(sensors_event_t& e, double dt);
  void ApplyStepCounter(sensors_event_t& e, double dt);
  void ApplyStepDetector(sensors_event_t& e, double dt);

private:
  Params params_;
  State state_;
};

}  // namespace gait