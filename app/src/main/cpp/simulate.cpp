#include "simulate.h"

#include <android/log.h>
#include <cmath>
#include <cstdint>

#define LOG_TAG "GaitSim"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace gait {

static constexpr double kTwoPi = 6.283185307179586476925286766559;
static constexpr double kNsToSec = 1e-9;
static constexpr double kMaxDeltaSec = 0.050;

static float Clamp(float v, float lo, float hi) {
  return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static float ModeDefaultSpm(Mode m) {
  switch (m) {
    case Mode::Walk: return 120.0f;
    case Mode::Run: return 165.0f;
    case Mode::FastRun: return 200.0f;
    default: return 120.0f;
  }
}

Simulator::Simulator() {
  params_.steps_per_minute = 120.0f;
  params_.mode = Mode::Walk;
  params_.enable = true;
  state_.current_spm = params_.steps_per_minute;
  state_.target_spm = params_.steps_per_minute;
}

void Simulator::UpdateParams(const Params& p) {
  params_ = p;

  float spm = p.steps_per_minute;
  if (spm <= 0.0f) {
    spm = ModeDefaultSpm(p.mode);
  }
  spm = Clamp(spm, 30.0f, 300.0f);
  state_.target_spm = spm;
}

const Params& Simulator::GetParams() const {
  return params_;
}

double Simulator::NextUniform01() {
  uint64_t x = state_.rng_state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  state_.rng_state = x;
  uint64_t r = x * 2685821657736338717ULL;
  const double u = (r >> 11) * (1.0 / 9007199254740992.0);
  return u;
}

double Simulator::NextSignedNoise(double amplitude) {
  double u = NextUniform01();
  double v = (u * 2.0) - 1.0;
  return v * amplitude;
}

void Simulator::EnsureInitialized(int64_t ts_ns) {
  if (state_.initialized) return;
  state_.initialized = true;
  state_.last_ts_ns = ts_ns;
  state_.phase = 0.0;
  state_.step_counter = 0.0;
  state_.step_phase_acc = 0.0;

  uint64_t seed = static_cast<uint64_t>(ts_ns) ^ 0x9E3779B97F4A7C15ULL;
  if (seed == 0) seed = 0xCAFEBABEULL;
  state_.rng_state = seed;

  state_.current_spm = state_.target_spm;
  ALOGI("Simulator initialized, spm=%.2f enable=%d", state_.current_spm, params_.enable ? 1 : 0);
}

void Simulator::SmoothStepRate(double dt) {
  const double tau = 1.2;
  const double alpha = 1.0 - std::exp(-dt / tau);
  state_.current_spm = static_cast<float>(
      state_.current_spm + (state_.target_spm - state_.current_spm) * alpha);
}

void Simulator::AdvancePhase(double dt) {
  const double sps = static_cast<double>(state_.current_spm) / 60.0;
  const double omega = kTwoPi * sps;

  double omega_jitter = omega;
  omega_jitter *= 1.0 + NextSignedNoise(0.015);

  state_.phase += omega_jitter * dt;
  if (state_.phase > 1e9) state_.phase = std::fmod(state_.phase, kTwoPi);

  state_.step_phase_acc += sps * dt;
}

void Simulator::ApplyAccelerometer(sensors_event_t& e, double dt) {
  (void)dt;

  const double sps = static_cast<double>(state_.current_spm) / 60.0;
  const double omega = kTwoPi * sps;

  double a = 1.0;
  double b = 1.0;
  double c = 1.0;
  switch (params_.mode) {
    case Mode::Walk:
      a = 1.0; b = 1.0; c = 1.0;
      break;
    case Mode::Run:
      a = 1.15; b = 1.10; c = 1.10;
      break;
    case Mode::FastRun:
      a = 1.30; b = 1.20; c = 1.20;
      break;
  }

  double t = static_cast<double>(e.timestamp) * kNsToSec;

  double base_x = std::sin(omega * t) * 0.6 * a;
  double base_y = std::cos(omega * t) * 0.4 * b;
  double base_z = 9.8 + std::sin(2.0 * omega * t) * 0.2 * c;

  double noise_scale = 0.05;
  double x = base_x * (1.0 + NextSignedNoise(noise_scale));
  double y = base_y * (1.0 + NextSignedNoise(noise_scale));
  double z = base_z * (1.0 + NextSignedNoise(noise_scale));

  e.data[0] = static_cast<float>(x);
  e.data[1] = static_cast<float>(y);
  e.data[2] = static_cast<float>(z);
}

void Simulator::ApplyStepCounter(sensors_event_t& e, double dt) {
  const double sps = static_cast<double>(state_.current_spm) / 60.0;
  state_.step_counter += sps * dt;

  double drift = NextSignedNoise(0.0005) * (sps * dt);
  state_.step_counter += drift;

  if (state_.step_counter < 0.0) state_.step_counter = 0.0;
  e.data[0] = static_cast<float>(state_.step_counter);
}

void Simulator::ApplyStepDetector(sensors_event_t& e, double dt) {
  e.data[0] = 0.0f;

  int triggers = 0;
  while (state_.step_phase_acc >= 1.0) {
    state_.step_phase_acc -= 1.0;
    triggers++;
  }

  if (triggers > 0) {
    e.data[0] = 1.0f;
  }

  double micro = NextSignedNoise(0.002);
  state_.step_phase_acc += micro * dt;
}

void Simulator::ProcessEvents(sensors_event_t* events, std::size_t count) {
  if (!events || count == 0) return;

  int64_t last_ts = events[count - 1].timestamp;
  EnsureInitialized(last_ts);

  for (std::size_t i = 0; i < count; i++) {
    sensors_event_t& e = events[i];
    if (!params_.enable) continue;

    int64_t ts = e.timestamp;
    int64_t delta_ns = ts - state_.last_ts_ns;
    state_.last_ts_ns = ts;

    double dt = static_cast<double>(delta_ns) * kNsToSec;
    if (dt < 0.0) dt = 0.0;
    if (dt > kMaxDeltaSec) dt = kMaxDeltaSec;

    SmoothStepRate(dt);
    AdvancePhase(dt);

    switch (e.type) {
      case TYPE_ACCELEROMETER:
        ApplyAccelerometer(e, dt);
        break;
      case TYPE_STEP_COUNTER:
        ApplyStepCounter(e, dt);
        break;
      case TYPE_STEP_DETECTOR:
        ApplyStepDetector(e, dt);
        break;
      default:
        break;
    }
  }
}

}  // namespace gait