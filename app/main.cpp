#include "engine/camera/camera.hpp"
#include "engine/input/input.hpp"
#include "engine/map/map.hpp"
#include "engine/renderer/raycast.hpp"
#include "engine/timing/timing.hpp"
#include "game/player.hpp"
#include "ai/ai.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr int kWinW = 960;
constexpr int kWinH = 600;
constexpr int kFbW = 320;
constexpr int kFbH = 200;

std::string find_map_path() {
  const char* env = std::getenv("DOOM_GROK_ROOT");
  const char* candidates[] = {
    "levels/demo.map",
    "../levels/demo.map",
    "./levels/demo.map",
  };
  if (env && env[0]) {
    std::string p = std::string(env) + "/levels/demo.map";
    FILE* f = std::fopen(p.c_str(), "rb");
    if (f) { std::fclose(f); return p; }
  }
  for (const char* c : candidates) {
    FILE* f = std::fopen(c, "rb");
    if (f) { std::fclose(f); return c; }
  }
  return "levels/demo.map";
}

// Brief white flash + center crosshair tick for hitscan feedback.
void apply_muzzle_flash(Renderer::FrameBuffer& fb, float strength) {
  if (strength <= 0.0f || fb.width <= 0 || fb.height <= 0) return;
  strength = std::clamp(strength, 0.0f, 1.0f);
  const int w = fb.width;
  const int h = fb.height;
  const uint32_t add = static_cast<uint32_t>(180.0f * strength);
  for (int i = 0; i < w * h; ++i) {
    uint32_t p = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = std::min(255u, ((p >> 16) & 0xFFu) + add);
    uint32_t g = std::min(255u, ((p >> 8) & 0xFFu) + add);
    uint32_t b = std::min(255u, (p & 0xFFu) + add);
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  // Small center marker
  const int cx = w / 2;
  const int cy = h / 2;
  const uint32_t mark = 0xFFFFFFFFu;
  for (int dx = -2; dx <= 2; ++dx) {
    const int x = cx + dx;
    if (x >= 0 && x < w) fb.pixels[static_cast<size_t>(cy * w + x)] = mark;
  }
  for (int dy = -2; dy <= 2; ++dy) {
    const int y = cy + dy;
    if (y >= 0 && y < h) fb.pixels[static_cast<size_t>(y * w + cx)] = mark;
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "Doom-Grok",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kWinW, kWinH,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    renderer = SDL_CreateRenderer(window, -1, 0);
  }
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  SDL_Texture* texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kFbW, kFbH);
  if (!texture) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Map map;
  const std::string map_path = find_map_path();
  if (!map.load(map_path.c_str())) {
    std::fprintf(stderr, "Failed to load map: %s\n", map_path.c_str());
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  std::printf("Loaded map %s (%dx%d)\n", map_path.c_str(), map.width(), map.height());

  Player player;
  Camera camera;
  Vec2 start{2.5f, 2.5f};
  Vec2 facing{1.0f, 0.0f};
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::PlayerStart) {
      start = s.pos;
      break;
    }
  }
  player.set_pose(start, facing);
  player.sync_camera(camera);

  // AI stubs from monster spawns
  std::vector<AI::Agent> agents;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Monster) {
      AI::Agent a;
      a.type_id = s.type_id;
      a.x = s.pos.x;
      a.y = s.pos.y;
      agents.push_back(a);
    }
  }

  Input input;
  Timing::FixedStep clock;
  Renderer::FrameBuffer fb;
  fb.resize(kFbW, kFbH);

  SDL_SetRelativeMouseMode(SDL_TRUE);

  bool running = true;
  Uint64 prev = SDL_GetPerformanceCounter();
  const Uint64 freq = SDL_GetPerformanceFrequency();

  while (running) {
    input.begin_frame();
    bool mouse_fire = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        mouse_fire = true;
      }
      input.feed_sdl(e);
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    const double frame_dt = static_cast<double>(now - prev) / static_cast<double>(freq);
    prev = now;

    const int steps = clock.consume(frame_dt);
    const float dt = static_cast<float>(Timing::kFixedDt);

    // Look once per rendered frame, then sync camera.
    player.apply_look(input, input.mouse_delta().x);

    for (int i = 0; i < steps; ++i) {
      player.update(map, input, dt);
      for (auto& a : agents) AI::tick_stub(a, dt);
    }

    // Hitscan: edge-triggered (LMB this frame or Space/Ctrl pressed)
    player.try_fire(map, input, mouse_fire);
    player.sync_camera(camera);
    player.tick_fx(static_cast<float>(frame_dt));

    Renderer::raycast_view(map, camera, fb);
    if (player.muzzle_flash > 0.0f) {
      apply_muzzle_flash(fb, player.muzzle_flash / 0.08f);
    }
    Renderer::present(renderer, texture, fb);
  }

  SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
