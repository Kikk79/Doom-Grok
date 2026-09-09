#include "engine/audio/audio.hpp"

#include <SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Audio {
namespace {

constexpr int kSampleRate = 22050;
constexpr int kChannels = 1;
constexpr SDL_AudioFormat kFormat = AUDIO_S16SYS;

SDL_AudioDeviceID g_dev = 0;
bool g_ready = false;

struct Clip {
  std::vector<int16_t> samples;
};

Clip g_clips[6];

float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void push_sine(std::vector<int16_t>& out, float freq_hz, float dur_sec, float amp,
               float attack = 0.005f, float release = 0.02f) {
  const int n = static_cast<int>(dur_sec * static_cast<float>(kSampleRate));
  const int a_n = static_cast<int>(attack * static_cast<float>(kSampleRate));
  const int r_n = static_cast<int>(release * static_cast<float>(kSampleRate));
  for (int i = 0; i < n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
    float env = 1.0f;
    if (a_n > 0 && i < a_n) env = static_cast<float>(i) / static_cast<float>(a_n);
    if (r_n > 0 && i > n - r_n) {
      env *= static_cast<float>(n - i) / static_cast<float>(r_n);
    }
    const float s = std::sin(2.0f * 3.14159265f * freq_hz * t) * amp * env;
    out.push_back(static_cast<int16_t>(clampf(s, -1.0f, 1.0f) * 32000.0f));
  }
}

void push_noise(std::vector<int16_t>& out, float dur_sec, float amp,
                float attack = 0.002f, float release = 0.03f) {
  const int n = static_cast<int>(dur_sec * static_cast<float>(kSampleRate));
  const int a_n = static_cast<int>(attack * static_cast<float>(kSampleRate));
  const int r_n = static_cast<int>(release * static_cast<float>(kSampleRate));
  uint32_t rng = 0xA5A5F00Du;
  for (int i = 0; i < n; ++i) {
    rng = rng * 1664525u + 1013904223u;
    const float noise = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
    float env = 1.0f;
    if (a_n > 0 && i < a_n) env = static_cast<float>(i) / static_cast<float>(a_n);
    if (r_n > 0 && i > n - r_n) {
      env *= static_cast<float>(n - i) / static_cast<float>(r_n);
    }
    const float s = noise * amp * env;
    out.push_back(static_cast<int16_t>(clampf(s, -1.0f, 1.0f) * 28000.0f));
  }
}

void push_sweep(std::vector<int16_t>& out, float f0, float f1, float dur_sec, float amp) {
  const int n = static_cast<int>(dur_sec * static_cast<float>(kSampleRate));
  float phase = 0.0f;
  for (int i = 0; i < n; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>(n > 1 ? n - 1 : 1);
    const float freq = f0 + (f1 - f0) * u;
    float env = 1.0f;
    if (i < n / 10) env = static_cast<float>(i) / static_cast<float>(n / 10);
    if (i > (9 * n) / 10) {
      env *= static_cast<float>(n - i) / static_cast<float>(n / 10 + 1);
    }
    phase += 2.0f * 3.14159265f * freq / static_cast<float>(kSampleRate);
    const float s = std::sin(phase) * amp * env;
    out.push_back(static_cast<int16_t>(clampf(s, -1.0f, 1.0f) * 30000.0f));
  }
}

void build_clips() {
  {
    Clip& c = g_clips[static_cast<int>(Sfx::Fire)];
    c.samples.clear();
    push_noise(c.samples, 0.045f, 0.55f, 0.001f, 0.025f);
    push_sine(c.samples, 900.0f, 0.035f, 0.25f, 0.001f, 0.02f);
  }
  {
    Clip& c = g_clips[static_cast<int>(Sfx::Door)];
    c.samples.clear();
    push_sweep(c.samples, 220.0f, 140.0f, 0.16f, 0.4f);
    push_sine(c.samples, 90.0f, 0.06f, 0.2f, 0.005f, 0.04f);
  }
  {
    Clip& c = g_clips[static_cast<int>(Sfx::Hurt)];
    c.samples.clear();
    push_sine(c.samples, 120.0f, 0.08f, 0.5f, 0.001f, 0.02f);
    push_noise(c.samples, 0.07f, 0.35f, 0.001f, 0.04f);
  }
  {
    Clip& c = g_clips[static_cast<int>(Sfx::Pickup)];
    c.samples.clear();
    push_sine(c.samples, 660.0f, 0.05f, 0.35f, 0.002f, 0.02f);
    push_sine(c.samples, 990.0f, 0.07f, 0.3f, 0.002f, 0.03f);
  }
  {
    Clip& c = g_clips[static_cast<int>(Sfx::MonsterDeath)];
    c.samples.clear();
    push_sweep(c.samples, 380.0f, 80.0f, 0.18f, 0.45f);
    push_noise(c.samples, 0.1f, 0.3f, 0.001f, 0.05f);
  }
  {
    Clip& c = g_clips[static_cast<int>(Sfx::Win)];
    c.samples.clear();
    push_sine(c.samples, 523.25f, 0.09f, 0.35f, 0.005f, 0.03f);
    push_sine(c.samples, 659.25f, 0.09f, 0.35f, 0.005f, 0.03f);
    push_sine(c.samples, 783.99f, 0.12f, 0.4f, 0.005f, 0.05f);
    push_sine(c.samples, 1046.5f, 0.18f, 0.3f, 0.005f, 0.08f);
  }
}

}  // namespace

bool init() {
  if (g_ready) return true;

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "Audio: SDL_INIT_AUDIO failed (%s) — muted\n", SDL_GetError());
    g_ready = false;
    g_dev = 0;
    return false;
  }

  build_clips();

  SDL_AudioSpec want{};
  SDL_AudioSpec have{};
  want.freq = kSampleRate;
  want.format = kFormat;
  want.channels = kChannels;
  want.samples = 512;
  want.callback = nullptr;
  want.userdata = nullptr;

  g_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (g_dev == 0) {
    std::fprintf(stderr, "Audio: OpenAudioDevice failed (%s) — muted\n", SDL_GetError());
    g_ready = false;
    return false;
  }

  if (have.format != kFormat || have.channels != kChannels || have.freq != kSampleRate) {
    std::fprintf(stderr,
                 "Audio: unexpected device format freq=%d fmt=%u ch=%d — muted\n",
                 have.freq, static_cast<unsigned>(have.format), have.channels);
    SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
    g_ready = false;
    return false;
  }

  SDL_PauseAudioDevice(g_dev, 0);
  g_ready = true;
  std::printf("Audio: ready (%d Hz mono S16)\n", kSampleRate);
  return true;
}

void shutdown() {
  if (g_dev != 0) {
    SDL_ClearQueuedAudio(g_dev);
    SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
  }
  g_ready = false;
}

bool available() { return g_ready && g_dev != 0; }

void play(Sfx s) {
  if (!available()) return;
  const int idx = static_cast<int>(s);
  if (idx < 0 || idx >= 6) return;
  const Clip& c = g_clips[idx];
  if (c.samples.empty()) return;
  const Uint32 queued = SDL_GetQueuedAudioSize(g_dev);
  constexpr Uint32 kMaxQueued = static_cast<Uint32>(kSampleRate * sizeof(int16_t) / 2);
  if (queued > kMaxQueued) {
    SDL_ClearQueuedAudio(g_dev);
  }
  SDL_QueueAudio(g_dev, c.samples.data(),
                 static_cast<Uint32>(c.samples.size() * sizeof(int16_t)));
}

}  // namespace Audio
