#pragma once
#include "engine/camera/camera.hpp"
#include "engine/map/map.hpp"
#include "engine/renderer/raycast.hpp"

#include <cstdint>
#include <vector>

// Slice 4–5 — monsters: Idle → Chase → Attack; hitscan; melee; billboards.
namespace AI {

enum class State : uint8_t { Idle = 0, Chase = 1, Attack = 2 };

struct Monster {
  int type_id = 0;
  Vec2 pos{0.0f, 0.0f};
  int hp = 0;
  int max_hp = 0;
  float speed = 1.5f;
  float radius = 0.25f;
  State state = State::Idle;
  bool alive = true;
  uint32_t color = 0xFF5080C0u;  // default greenish/blue
  float attack_cd = 0.0f;        // seconds until next melee swing
};

// Melee tuning (Slice 5).
constexpr float kMeleeRange = 0.9f;
constexpr float kMeleeCooldown = 0.7f;
constexpr int kMeleeDamage = 10;

// Fill defaults from map spawn type_id (1 = Imp-like, 2 / other = tougher/slower).
Monster make_monster(int type_id, Vec2 pos);

// Spawn all Kind::Monster entries from map.spawns().
std::vector<Monster> spawn_from_map(const Map& map);

// Per-frame AI: LOS → Chase; close + (LOS|chasing) → Attack + melee.
// Returns total melee damage dealt to the player this tick (0 if none).
int update(std::vector<Monster>& monsters, const Map& map, Vec2 player_pos, float dt);

// Hitscan along ray: closest alive monster within radius before a wall.
// Reduces HP; sets alive=false when hp <= 0. Returns true if a monster was hit.
bool apply_hitscan(const Map& map, std::vector<Monster>& monsters, Vec2 from, Vec2 dir,
                   int damage);

// Classic camera-space billboards (colored vertical strip). Skips if wall occludes
// (Collision::hits_wall / raycast cam → monster). FrameBuffer from raycast.hpp.
void draw(Renderer::FrameBuffer& fb, const Camera& camera, const Map& map,
          const std::vector<Monster>& monsters);

}  // namespace AI
