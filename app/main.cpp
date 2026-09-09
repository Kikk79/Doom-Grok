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

void apply_pickup_effect(Player& player, int type_id) {
  if (type_id == 10) {
    player.health = std::min(200, player.health + 25);
    std::printf("pickup health +25 -> health=%d\n", player.health);
  } else if (type_id == 11) {
    player.armor = std::min(100, player.armor + 10);
    player.ammo += 10;
    std::printf("pickup armor/ammo +10 -> armor=%d ammo=%d\n",
                player.armor, player.ammo);
  } else {
    std::printf("pickup type_id=%d (no effect)\n", type_id);
  }
}

void collect_pickups(Player& player, std::vector<ActivePickup>& pickups) {
  const float r2 = kPickupRadius * kPickupRadius;
  for (size_t i = 0; i < pickups.size();) {
    const float dx = player.pos.x - pickups[i].pos.x;
    const float dy = player.pos.y - pickups[i].pos.y;
    if (dx * dx + dy * dy < r2) {
      apply_pickup_effect(player, pickups[i].type_id);
      pickups.erase(pickups.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

void draw_hud(Renderer::FrameBuffer& fb, const Player& player) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  auto fill_rect = [&](int x0, int y0, int rw, int rh, uint32_t color) {
    for (int y = y0; y < y0 + rh; ++y) {
      if (y < 0 || y >= h) continue;
      for (int x = x0; x < x0 + rw; ++x) {
        if (x < 0 || x >= w) continue;
        fb.pixels[static_cast<size_t>(y * w + x)] = color;
      }
    }
  };
  constexpr int kBarW = 80;
  constexpr int kBarH = 6;
  const int x0 = 4;
  const int y_hp = h - 12;
  const int y_ar = h - 20;
  fill_rect(x0, y_hp, kBarW, kBarH, 0xFF202020u);
  const int hp_fill = std::clamp(player.health, 0, 200) * (kBarW - 2) / 200;
  const uint32_t hp_col = player.health > 25 ? 0xFF40C040u : 0xFFC04040u;
  fill_rect(x0 + 1, y_hp + 1, hp_fill, kBarH - 2, hp_col);
  fill_rect(x0, y_ar, kBarW, 4, 0xFF202020u);
  const int ar_fill = std::clamp(player.armor, 0, 100) * (kBarW - 2) / 100;
  fill_rect(x0 + 1, y_ar + 1, ar_fill, 2, 0xFF4080C0u);
}

int run_smoke(const char* map_path) {
  Map map;
  if (!map.load(map_path)) {
    std::fprintf(stderr, "SMOKE FAIL: load %s\n", map_path);
    return 1;
  }
  int door_tx = -1, door_ty = -1;
  for (int y = 0; y < map.height(); ++y) {
    for (int x = 0; x < map.width(); ++x) {
      if (map.tile_at(x, y) == Tile::DoorClosed) {
        door_tx = x; door_ty = y; break;
      }
    }
    if (door_tx >= 0) break;
  }
  if (door_tx < 0) {
    std::fprintf(stderr, "SMOKE FAIL: no DoorClosed in map\n");
    return 1;
  }
  if (!map.try_open_door(door_tx, door_ty)) {
    std::fprintf(stderr, "SMOKE FAIL: try_open_door\n");
    return 1;
  }
  if (map.tile_at(door_tx, door_ty) != Tile::DoorOpen || map.is_solid(door_tx, door_ty)) {
    std::fprintf(stderr, "SMOKE FAIL: DoorOpen still solid or wrong tile\n");
    return 1;
  }
  std::printf("smoke: door (%d,%d) opened, walkable=%d\n",
              door_tx, door_ty, map.is_solid(door_tx, door_ty) ? 0 : 1);

  Player player;
  player.health = 100;
  player.armor = 0;
  std::vector<ActivePickup> pickups;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Item) pickups.push_back({s.pos, s.type_id});
  }
  if (pickups.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: no item spawns\n");
    return 1;
  }
  const size_t before = pickups.size();
  player.pos = pickups[0].pos;
  collect_pickups(player, pickups);
  if (pickups.size() != before - 1) {
    std::fprintf(stderr, "SMOKE FAIL: pickup not collected\n");
    return 1;
  }
  if (player.health < 100 && player.armor <= 0 && player.ammo <= 0) {
    std::fprintf(stderr, "SMOKE FAIL: no pickup effect applied\n");
    return 1;
  }
  std::printf("smoke: pickups %zu -> %zu health=%d armor=%d ammo=%d\n",
              before, pickups.size(), player.health, player.armor, player.ammo);
  if (map.set_tile(-1, 0, Tile::Empty) || map.set_tile(map.width(), 0, Tile::Empty)) {
    std::fprintf(stderr, "SMOKE FAIL: OOB set_tile should fail\n");
    return 1;
  }
  if (map.try_open_door(-1, 0) || map.try_open_door(0, 0)) {
    std::fprintf(stderr, "SMOKE FAIL: try_open_door should reject non-doors\n");
    return 1;
  }

  // --- Slice 4 (light): spawn + Chase on LOS + hitscan kill ---
  std::vector<AI::Monster> monsters = AI::spawn_from_map(map);
  if (monsters.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: expected monster spawns\n");
    return 1;
  }
  std::printf("smoke: spawned %zu monsters\n", monsters.size());
  for (const auto& m : monsters) {
    if (!m.alive || m.hp != m.max_hp || m.max_hp <= 0) {
      std::fprintf(stderr, "SMOKE FAIL: monster hp init type_id=%d\n", m.type_id);
      return 1;
    }
  }

  AI::Monster& prey = monsters[0];
  player.pos = {prey.pos.x - 2.0f, prey.pos.y};
  player.dir = {1.0f, 0.0f};
  if (Collision::hits_wall(map, prey.pos, player.pos)) {
    player.pos = {7.5f, 8.5f};
    prey.pos = {9.5f, 8.5f};
  }
  const float dist0 = std::hypot(prey.pos.x - player.pos.x, prey.pos.y - player.pos.y);
  for (int i = 0; i < 60; ++i) {
    AI::update(monsters, map, player.pos, 1.0f / 60.0f);
  }
  if (prey.state != AI::State::Chase) {
    std::fprintf(stderr, "SMOKE FAIL: monster did not enter Chase on LOS\n");
    return 1;
  }
  const float dist1 = std::hypot(prey.pos.x - player.pos.x, prey.pos.y - player.pos.y);
  if (!(dist1 < dist0 - 0.05f)) {
    std::fprintf(stderr, "SMOKE FAIL: chase did not close (%.3f -> %.3f)\n", dist0, dist1);
    return 1;
  }
  std::printf("smoke: chase ok dist %.3f -> %.3f\n", dist0, dist1);

  player.pos = {prey.pos.x - 1.5f, prey.pos.y};
  player.dir = {1.0f, 0.0f};
  prey.alive = true;
  prey.hp = prey.max_hp;
  int shots = 0;
  while (prey.alive && shots < 16) {
    if (!AI::apply_hitscan(map, monsters, player.pos, player.dir, 25)) {
      std::fprintf(stderr, "SMOKE FAIL: hitscan missed living monster\n");
      return 1;
    }
    ++shots;
  }
  if (prey.alive) {
    std::fprintf(stderr, "SMOKE FAIL: monster not killed after %d shots\n", shots);
    return 1;
  }
  std::printf("smoke: hitscan kill in %d shots\n", shots);

  std::printf("SMOKE OK\n");
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string map_path = find_map_path();
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--smoke") return run_smoke(map_path.c_str());
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
  if (!map.load(map_path.c_str())) {
    std::fprintf(stderr, "Failed to load map: %s\n", map_path.c_str());
    SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); SDL_Quit(); return 1;
  }
  std::printf("Loaded map %s (%dx%d)\n", map_path.c_str(), map.width(), map.height());

  Player player;
  Camera camera;
  Vec2 start{2.5f, 2.5f};
  Vec2 facing{1.0f, 0.0f};
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::PlayerStart) { start = s.pos; break; }
  }
  player.set_pose(start, facing);
  player.sync_camera(camera);

  // Slice 4: monsters from Map::spawns Kind::Monster + type_id.
  std::vector<AI::Monster> monsters = AI::spawn_from_map(map);
  std::printf("Monsters spawned: %zu\n", monsters.size());

  std::vector<ActivePickup> pickups;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Item) pickups.push_back({s.pos, s.type_id});
  }
  std::printf("Active pickups: %zu  health=%d\n", pickups.size(), player.health);

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
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) mouse_fire = true;
      input.feed_sdl(e);
    }
    const Uint64 now = SDL_GetPerformanceCounter();
    const double frame_dt = static_cast<double>(now - prev) / static_cast<double>(freq);
    prev = now;
    const int steps = clock.consume(frame_dt);
    const float dt = static_cast<float>(Timing::kFixedDt);
    player.apply_look(input, input.mouse_delta().x);
    for (int i = 0; i < steps; ++i) {
      player.update(map, input, dt);
      collect_pickups(player, pickups);
      AI::update(monsters, map, player.pos, dt);
    }
    player.try_use(map, input);
    if (player.try_fire(map, input, mouse_fire)) {
      if (AI::apply_hitscan(map, monsters, player.pos, player.dir, 25)) {
        std::printf("hitscan monster hit\n");
      }
    }
    player.sync_camera(camera);
    player.tick_fx(static_cast<float>(frame_dt));
    Renderer::raycast_view(map, camera, fb);
    AI::draw(fb, camera, map, monsters);
    if (player.muzzle_flash > 0.0f) apply_muzzle_flash(fb, player.muzzle_flash / 0.08f);
    draw_hud(fb, player);
    Renderer::present(renderer, texture, fb);
  }
  SDL_SetRelativeMouseMode(SDL_FALSE);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
