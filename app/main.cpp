#include "engine/camera/camera.hpp"
#include "engine/collision/collision.hpp"
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
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr int kWinW = 960;
constexpr int kWinH = 600;
constexpr int kFbW = 320;
constexpr int kFbH = 200;
constexpr float kPickupRadius = 0.5f;

struct ActivePickup {
  Vec2 pos;
  int type_id;
};

// Slice 8 — fixed campaign order (relative names; resolved at runtime).
constexpr const char* kCampaignLevels[] = {
  "levels/demo.map",
  "levels/e1m2.map",
};
constexpr int kCampaignCount = 2;

bool file_readable(const std::string& p) {
  FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  std::fclose(f);
  return true;
}

// Resolve a levels/... relative path via DOOM_GROK_ROOT / cwd / ../.
std::string resolve_map_path(const char* relative) {
  const char* env = std::getenv("DOOM_GROK_ROOT");
  if (env && env[0]) {
    std::string p = std::string(env) + "/" + relative;
    if (file_readable(p)) return p;
  }
  const char* prefixes[] = {"", "./", "../"};
  for (const char* pre : prefixes) {
    std::string p = std::string(pre) + relative;
    if (file_readable(p)) return p;
  }
  // Basename fallback (binary-next-to-levels after CMake copy).
  const char* slash = std::strrchr(relative, '/');
  const char* base = slash ? slash + 1 : relative;
  std::string near = std::string("levels/") + base;
  if (file_readable(near)) return near;
  return relative;
}

std::string find_map_path() {
  return resolve_map_path(kCampaignLevels[0]);
}

std::string map_basename(const std::string& path) {
  const auto slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

int campaign_index_for(const std::string& path) {
  const std::string base = map_basename(path);
  for (int i = 0; i < kCampaignCount; ++i) {
    if (base == map_basename(kCampaignLevels[i])) return i;
  }
  return -1;
}

// Empty string => current is last map (campaign complete).
std::string next_campaign_path(const std::string& current) {
  const int idx = campaign_index_for(current);
  if (idx < 0 || idx + 1 >= kCampaignCount) return {};
  return resolve_map_path(kCampaignLevels[idx + 1]);
}

#include "main_rest_a.inc"
#include "main_rest_b.inc"
#include "main_rest_c.inc"
}  // namespace

int main(int argc, char** argv) {
  std::string current_level = find_map_path();
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--smoke") return run_smoke(current_level.c_str());
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow(
      "Doom-Grok", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kWinW, kWinH, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit(); return 1;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window); SDL_Quit(); return 1;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_Texture* texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kFbW, kFbH);
  if (!texture) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 1;
  }
  Map map;
  Player player;
  Camera camera;
  std::vector<AI::Monster> monsters;
  std::vector<ActivePickup> pickups;
  if (!reload_level(current_level.c_str(), map, player, camera, monsters, pickups)) {
    std::fprintf(stderr, "Failed to load map: %s\n", current_level.c_str());
    SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); SDL_Quit(); return 1;
  }
  std::printf("Loaded map %s (%dx%d)\n", current_level.c_str(), map.width(), map.height());
  std::printf("Monsters spawned: %zu\n", monsters.size());
  std::printf("Active pickups: %zu  health=%d\n", pickups.size(), player.health);

  Input input;
  Timing::FixedStep clock;
  Renderer::FrameBuffer fb;
  fb.resize(kFbW, kFbH);
  SDL_SetRelativeMouseMode(SDL_TRUE);
  bool running = true;
  bool game_over_logged = false;
  bool won = false;
  bool win_logged = false;
  Uint64 prev = SDL_GetPerformanceCounter();
  const Uint64 freq = SDL_GetPerformanceFrequency();
  while (running) {
    input.begin_frame();
    bool mouse_fire = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) mouse_fire = true;
      input.feed_sdl(e);
    }
    const Uint64 now = SDL_GetPerformanceCounter();
    const double frame_dt = static_cast<double>(now - prev) / static_cast<double>(freq);
    prev = now;

    if (player.is_dead()) {
      won = false;
      if (!game_over_logged) {
        std::printf("GAME OVER — press R to restart\n");
        game_over_logged = true;
      }
      if (input.key_pressed(SDL_SCANCODE_R)) {
        if (reload_level(current_level.c_str(), map, player, camera, monsters, pickups)) {
          game_over_logged = false;
          win_logged = false;
          won = false;
          clock.reset();
        }
      }
      player.tick_fx(static_cast<float>(frame_dt));
      Renderer::raycast_view(map, camera, fb);
      AI::draw(fb, camera, map, monsters);
      draw_hud(fb, player);
      draw_game_over(fb);
      Renderer::present(renderer, texture, fb);
      continue;
    }

    if (won) {
      const std::string next_path = next_campaign_path(current_level);
      const bool campaign_done = next_path.empty();
      if (!win_logged) {
        if (campaign_done) {
          std::printf("CAMPAIGN COMPLETE — press R to restart e1m2 (N does nothing)\n");
        } else {
          std::printf("YOU WIN — press N for next map, R to restart current\n");
        }
        win_logged = true;
      }
      // R: always full restart of the *current* level (no wrap to demo).
      if (input.key_pressed(SDL_SCANCODE_R)) {
        if (reload_level(current_level.c_str(), map, player, camera, monsters, pickups)) {
          won = false;
          win_logged = false;
          game_over_logged = false;
          clock.reset();
        }
      }
      // N: load next map fresh after demo win; no-op on final map (stay on COMPLETE).
      if (input.key_pressed(SDL_SCANCODE_N) && !campaign_done) {
        current_level = next_path;
        if (reload_level(current_level.c_str(), map, player, camera, monsters, pickups)) {
          won = false;
          win_logged = false;
          game_over_logged = false;
          clock.reset();
          std::printf("Loaded next map %s (%dx%d)\n",
                      current_level.c_str(), map.width(), map.height());
        }
      }
      player.tick_fx(static_cast<float>(frame_dt));
      Renderer::raycast_view(map, camera, fb);
      AI::draw(fb, camera, map, monsters);
      draw_hud(fb, player);
      if (campaign_done) draw_campaign_complete(fb);
      else draw_you_win(fb);
      Renderer::present(renderer, texture, fb);
      continue;
    }

    const int steps = clock.consume(frame_dt);
    const float dt = static_cast<float>(Timing::kFixedDt);
    player.apply_look(input, input.mouse_delta().x);
    for (int i = 0; i < steps; ++i) {
      player.update(map, input, dt);
      collect_pickups(player, pickups);
      AI::update(monsters, map, player, dt);
    }
    player.try_use(map, input);
    if (player.try_fire(map, input, mouse_fire)) {
      if (AI::apply_hitscan(map, monsters, player.pos, player.dir, 25)) {
        std::printf("hitscan monster hit\n");
      }
    }
    // Slice 6: all monsters dead → win (exit after clear also satisfies check_win).
    if (check_win(monsters, player, map)) {
      won = true;
    }
    player.sync_camera(camera);
    player.tick_fx(static_cast<float>(frame_dt));
    Renderer::raycast_view(map, camera, fb);
    AI::draw(fb, camera, map, monsters);
    if (player.muzzle_flash > 0.0f) apply_muzzle_flash(fb, player.muzzle_flash / 0.08f);
    if (player.invulnerable()) apply_damage_flash(fb, player.invuln_t);
    draw_hud(fb, player);
    if (won) draw_you_win(fb);
    Renderer::present(renderer, texture, fb);
  }
  SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
