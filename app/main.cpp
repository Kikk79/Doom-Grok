#include "engine/camera/camera.hpp"
#include "engine/collision/collision.hpp"
#include "engine/input/input.hpp"
#include "engine/map/map.hpp"
#include "engine/renderer/raycast.hpp"
#include "engine/timing/timing.hpp"
#include "game/player.hpp"
#include "ai/ai.hpp"

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

void rotate_camera(Camera& cam, float angle) {
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  const Vec2 d = cam.dir;
  const Vec2 p = cam.plane;
  cam.dir = {d.x * c - d.y * s, d.x * s + d.y * c};
  cam.plane = {p.x * c - p.y * s, p.x * s + p.y * c};
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
  Vec2 start{2.5f, 2.5f};
  Vec2 facing{1.0f, 0.0f};
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::PlayerStart) {
      start = s.pos;
      break;
    }
  }
  player.camera.set_pose(start, facing);

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
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
      input.feed_sdl(e);
    }

    const Uint64 now = SDL_GetPerformanceCounter();
    const double frame_dt = static_cast<double>(now - prev) / static_cast<double>(freq);
    prev = now;

    const int steps = clock.consume(frame_dt);
    const float dt = static_cast<float>(Timing::kFixedDt);

    // Mouse look once per rendered frame (not per fixed step)
    {
      const Vec2 md = input.mouse_delta();
      if (md.x != 0.0f) rotate_camera(player.camera, md.x * player.mouse_sens);
    }

    for (int i = 0; i < steps; ++i) {
      // Turn with arrows
      float turn = 0.0f;
      if (input.key_down(SDL_SCANCODE_LEFT)) turn -= player.turn_speed * dt;
      if (input.key_down(SDL_SCANCODE_RIGHT)) turn += player.turn_speed * dt;
      if (turn != 0.0f) rotate_camera(player.camera, turn);

      // WASD + up/down arrows move in camera space
      Vec2 wish{0.0f, 0.0f};
      if (input.key_down(SDL_SCANCODE_W) || input.key_down(SDL_SCANCODE_UP)) {
        wish.x += player.camera.dir.x;
        wish.y += player.camera.dir.y;
      }
      if (input.key_down(SDL_SCANCODE_S) || input.key_down(SDL_SCANCODE_DOWN)) {
        wish.x -= player.camera.dir.x;
        wish.y -= player.camera.dir.y;
      }
      if (input.key_down(SDL_SCANCODE_A)) {
        wish.x -= player.camera.dir.y;
        wish.y += player.camera.dir.x;
      }
      if (input.key_down(SDL_SCANCODE_D)) {
        wish.x += player.camera.dir.y;
        wish.y -= player.camera.dir.x;
      }
      const float wlen = std::sqrt(wish.x * wish.x + wish.y * wish.y);
      Vec2 vel{0.0f, 0.0f};
      if (wlen > 1e-6f) {
        vel.x = (wish.x / wlen) * player.move_speed * dt;
        vel.y = (wish.y / wlen) * player.move_speed * dt;
      }
      player.camera.pos = Collision::move(map, player.camera.pos, vel, player.radius);

      for (auto& a : agents) AI::tick_stub(a, dt);
    }

    Renderer::raycast_view(map, player.camera, fb);
    Renderer::present(renderer, texture, fb);
  }

  SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
