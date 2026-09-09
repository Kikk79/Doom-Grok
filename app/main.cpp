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
  const bool blink_off = player.invulnerable() &&
                         (static_cast<int>(player.invuln_t * 10.0f) % 2 == 0);
  const uint32_t hp_col = blink_off ? 0xFF808080u
                                    : (player.health > 25 ? 0xFF40C040u : 0xFFC04040u);
  fill_rect(x0 + 1, y_hp + 1, hp_fill, kBarH - 2, hp_col);
  fill_rect(x0, y_ar, kBarW, 4, 0xFF202020u);
  const int ar_fill = std::clamp(player.armor, 0, 100) * (kBarW - 2) / 100;
  fill_rect(x0 + 1, y_ar + 1, ar_fill, 2, 0xFF4080C0u);
}


void draw_game_over(Renderer::FrameBuffer& fb) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  for (int i = 0; i < w * h; ++i) {
    uint32_t pix = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = ((pix >> 16) & 0xFFu) / 3;
    uint32_t g = ((pix >> 8) & 0xFFu) / 3;
    uint32_t b = (pix & 0xFFu) / 3;
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  const int y0 = h / 2 - 10;
  const int y1 = h / 2 + 10;
  for (int y = y0; y < y1; ++y) {
    if (y < 0 || y >= h) continue;
    for (int x = w / 5; x < (4 * w) / 5; ++x) {
      fb.pixels[static_cast<size_t>(y * w + x)] = 0xFFC01818u;
    }
  }
}

// Slice 6 — distinct from game over: green/gold banner + mild brighten (not darken).
void draw_you_win(Renderer::FrameBuffer& fb) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  for (int i = 0; i < w * h; ++i) {
    uint32_t pix = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = std::min(255u, ((pix >> 16) & 0xFFu) + 20u);
    uint32_t g = std::min(255u, ((pix >> 8) & 0xFFu) + 40u);
    uint32_t b = std::min(255u, (pix & 0xFFu) + 10u);
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  const int y0 = h / 2 - 10;
  const int y1 = h / 2 + 10;
  for (int y = y0; y < y1; ++y) {
    if (y < 0 || y >= h) continue;
    for (int x = w / 5; x < (4 * w) / 5; ++x) {
      fb.pixels[static_cast<size_t>(y * w + x)] = 0xFF18C040u;  // green
    }
  }
  // Gold accent stripe
  const int y_mid = h / 2;
  if (y_mid >= 0 && y_mid < h) {
    for (int x = w / 5; x < (4 * w) / 5; ++x) {
      fb.pixels[static_cast<size_t>(y_mid * w + x)] = 0xFFE0C020u;
    }
  }
}

bool all_monsters_dead(const std::vector<AI::Monster>& monsters) {
  if (monsters.empty()) return false;
  for (const auto& m : monsters) {
    if (m.alive) return false;
  }
  return true;
}

// EntitySpawn Kind::Door is map 'E' — exit marker (optional secondary win trigger).
bool near_exit(const Player& player, const Map& map) {
  const float r2 = kPickupRadius * kPickupRadius;
  for (const auto& s : map.spawns()) {
    if (s.kind != EntitySpawn::Kind::Door) continue;
    const float dx = player.pos.x - s.pos.x;
    const float dy = player.pos.y - s.pos.y;
    if (dx * dx + dy * dy < r2) return true;
  }
  return false;
}

bool check_win(const std::vector<AI::Monster>& monsters, const Player& player,
               const Map& map) {
  if (!player.alive()) return false;
  if (!all_monsters_dead(monsters)) return false;
  // All-dead is enough to win. Exit (E / Kind::Door) after clear is an alternate
  // trigger that yields the same flag (useful if win is deferred; kept for smoke).
  if (near_exit(player, map)) return true;
  return true;
}

void apply_damage_flash(Renderer::FrameBuffer& fb, float invuln_t) {
  if (invuln_t <= 0.0f || fb.width <= 0 || fb.height <= 0) return;
  const float strength = std::clamp(invuln_t / Player::kIFrameSec, 0.0f, 1.0f) * 0.55f;
  const int n = fb.width * fb.height;
  const uint32_t add_r = static_cast<uint32_t>(140.0f * strength);
  for (int i = 0; i < n; ++i) {
    uint32_t pix = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = std::min(255u, ((pix >> 16) & 0xFFu) + add_r);
    uint32_t g = static_cast<uint32_t>(static_cast<float>((pix >> 8) & 0xFFu) * (1.0f - 0.3f * strength));
    uint32_t b = static_cast<uint32_t>(static_cast<float>(pix & 0xFFu) * (1.0f - 0.3f * strength));
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
}

Vec2 find_player_start(const Map& map) {
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::PlayerStart) return s.pos;
  }
  return {2.5f, 2.5f};
}

void rebuild_pickups(const Map& map, std::vector<ActivePickup>& pickups) {
  pickups.clear();
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Item) pickups.push_back({s.pos, s.type_id});
  }
}

bool reload_level(const char* map_path, Map& map, Player& player, Camera& camera,
                  std::vector<AI::Monster>& monsters,
                  std::vector<ActivePickup>& pickups) {
  if (!map.load(map_path)) {
    std::fprintf(stderr, "reload_level: failed to load %s\n", map_path);
    return false;
  }
  player.reset_vitals(find_player_start(map), {1.0f, 0.0f});
  player.sync_camera(camera);
  monsters = AI::spawn_from_map(map);
  rebuild_pickups(map, pickups);
  std::printf("level restarted: monsters=%zu pickups=%zu health=%d\n",
              monsters.size(), pickups.size(), player.health);
  return true;
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
    AI::update(monsters, map, player, 1.0f / 60.0f);
  }
  // May already be Attack if chase closed inside melee range — both mean aggro.
  if (prey.state != AI::State::Chase && prey.state != AI::State::Attack) {
    std::fprintf(stderr, "SMOKE FAIL: monster did not enter Chase/Attack on LOS (state=%d)\n",
                 static_cast<int>(prey.state));
    return 1;
  }
  const float dist1 = std::hypot(prey.pos.x - player.pos.x, prey.pos.y - player.pos.y);
  if (!(dist1 < dist0 - 0.05f) && prey.state != AI::State::Attack) {
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

  // --- Slice 5: melee Attack calls Player::take_damage ---
  monsters = AI::spawn_from_map(map);
  AI::Monster& bruiser = monsters[0];
  bruiser.alive = true;
  bruiser.hp = bruiser.max_hp;
  bruiser.state = AI::State::Chase;
  bruiser.attack_cd = 0.0f;
  player.reset_vitals(player.pos, player.dir);
  player.armor = 0;
  player.health = 100;
  player.invuln_t = 0.0f;
  // Place player inside melee range with clear LOS.
  player.pos = {bruiser.pos.x + 0.4f, bruiser.pos.y};
  if (Collision::hits_wall(map, bruiser.pos, player.pos)) {
    bruiser.pos = {8.5f, 8.5f};
    player.pos = {9.0f, 8.5f};
  }
  const int expect_dmg = AI::melee_damage_for(bruiser.type_id);
  const int hp_before = player.health;
  AI::update(monsters, map, player, 1.0f / 60.0f);
  if (bruiser.state != AI::State::Attack) {
    std::fprintf(stderr, "SMOKE FAIL: expected Attack state got %d\n",
                 static_cast<int>(bruiser.state));
    return 1;
  }
  if (player.health != hp_before - expect_dmg) {
    std::fprintf(stderr, "SMOKE FAIL: melee dmg expected hp=%d got %d (type_id=%d dmg=%d)\n",
                 hp_before - expect_dmg, player.health, bruiser.type_id, expect_dmg);
    return 1;
  }
  if (bruiser.attack_cd <= 0.0f) {
    std::fprintf(stderr, "SMOKE FAIL: attack cooldown not set\n");
    return 1;
  }
  // Still on cooldown: no second swing even if i-frames cleared.
  player.invuln_t = 0.0f;
  const int hp_mid = player.health;
  AI::update(monsters, map, player, 0.1f);
  if (player.health != hp_mid) {
    std::fprintf(stderr, "SMOKE FAIL: melee fired during cooldown\n");
    return 1;
  }
  // Advance past cooldown → another hit.
  AI::update(monsters, map, player, AI::kMeleeCooldown);
  if (player.health != hp_mid - expect_dmg) {
    std::fprintf(stderr, "SMOKE FAIL: melee after cooldown expected hp=%d got %d\n",
                 hp_mid - expect_dmg, player.health);
    return 1;
  }
  std::printf("smoke: melee Attack ok dmg=%d cd=%.2f range=%.2f\n",
              expect_dmg, AI::kMeleeCooldown, AI::kMeleeRange);

  // i-frames: second direct take_damage blocked, then after >0.75s lands again.
  player.invuln_t = 0.0f;
  if (!player.take_damage(10) || !player.invulnerable()) {
    std::fprintf(stderr, "SMOKE FAIL: take_damage / i-frames arm\n");
    return 1;
  }
  if (player.take_damage(10)) {
    std::fprintf(stderr, "SMOKE FAIL: i-frames should block\n");
    return 1;
  }
  player.tick_fx(Player::kIFrameSec + 0.05f);
  if (player.invulnerable() || !player.take_damage(10)) {
    std::fprintf(stderr, "SMOKE FAIL: post-iframe damage\n");
    return 1;
  }
  std::printf("smoke: i-frames ok\n");

  // Death + restart
  player.invuln_t = 0.0f;
  player.armor = 0;
  player.health = 5;
  if (!player.take_damage(10) || player.alive()) {
    std::fprintf(stderr, "SMOKE FAIL: expected death\n");
    return 1;
  }
  Camera cam;
  if (!reload_level(map_path, map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload_level\n");
    return 1;
  }
  if (player.health != 100 || !player.alive() || monsters.empty() || pickups.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: restart state\n");
    return 1;
  }
  std::printf("smoke: death+restart ok\n");

  // --- Slice 6: kill all → win flag ---
  monsters = AI::spawn_from_map(map);
  if (monsters.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: no monsters for win test\n");
    return 1;
  }
  if (all_monsters_dead(monsters) || check_win(monsters, player, map)) {
    std::fprintf(stderr, "SMOKE FAIL: win should be false while monsters alive\n");
    return 1;
  }
  for (auto& m : monsters) {
    m.alive = false;
    m.hp = 0;
  }
  if (!all_monsters_dead(monsters)) {
    std::fprintf(stderr, "SMOKE FAIL: all_monsters_dead after kill-all\n");
    return 1;
  }
  const bool win_flag = check_win(monsters, player, map);
  if (!win_flag) {
    std::fprintf(stderr, "SMOKE FAIL: expected win flag after kill-all\n");
    return 1;
  }
  // Optional exit: standing on E after clear also wins (same flag).
  bool have_exit = false;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Door) {
      player.pos = s.pos;
      have_exit = true;
      break;
    }
  }
  if (have_exit && !check_win(monsters, player, map)) {
    std::fprintf(stderr, "SMOKE FAIL: exit-after-clear should win\n");
    return 1;
  }
  // R / reload clears win path (monsters respawn → not won).
  if (!reload_level(map_path, map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload after win\n");
    return 1;
  }
  if (all_monsters_dead(monsters) || check_win(monsters, player, map)) {
    std::fprintf(stderr, "SMOKE FAIL: win should clear after restart\n");
    return 1;
  }
  std::printf("smoke: win flag ok (kill-all + restart)%s\n",
              have_exit ? " + exit" : "");

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
  Player player;
  Camera camera;
  std::vector<AI::Monster> monsters;
  std::vector<ActivePickup> pickups;
  if (!reload_level(map_path.c_str(), map, player, camera, monsters, pickups)) {
    std::fprintf(stderr, "Failed to load map: %s\n", map_path.c_str());
    SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); SDL_Quit(); return 1;
  }
  std::printf("Loaded map %s (%dx%d)\n", map_path.c_str(), map.width(), map.height());
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
        if (reload_level(map_path.c_str(), map, player, camera, monsters, pickups)) {
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
      if (!win_logged) {
        std::printf("YOU WIN — press R to restart\n");
        win_logged = true;
      }
      if (input.key_pressed(SDL_SCANCODE_R)) {
        if (reload_level(map_path.c_str(), map, player, camera, monsters, pickups)) {
          won = false;
          win_logged = false;
          game_over_logged = false;
          clock.reset();
        }
      }
      player.tick_fx(static_cast<float>(frame_dt));
      Renderer::raycast_view(map, camera, fb);
      AI::draw(fb, camera, map, monsters);
      draw_hud(fb, player);
      draw_you_win(fb);
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
