#pragma once
#include "engine/camera/camera.hpp"
#include "engine/map/map.hpp"
#include "engine/renderer/raycast.hpp"
#include "game/player.hpp"

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

// Melee tuning (Slice 5 / TEAM-Leader).
constexpr float kMeleeRange = 0.55f;
constexpr float kMeleeCooldown = 0.9f;  // monster-side; Player i-frames also apply

// type_id == 2 → 15, else 10 (Imp-like / default).
inline int melee_damage_for(int type_id) { return type_id == 2 ? 15 : 10; }

// Fill defaults from map spawn type_id (1 = Imp-like, 2 / other = tougher/slower).
Monster make_monster(int type_id, Vec2 pos);

// Spawn all Kind::Monster entries from map.spawns().
std::vector<Monster> spawn_from_map(const Map& map);

// Per-frame AI: Idle → (LOS) Chase → (dist ≤ kMeleeRange + LOS|aggro) Attack.
// Calls player.take_damage on swing (respects Player i-frames).
void update(std::vector<Monster>& monsters, const Map& map, Player& player, float dt);

// Hitscan along ray: closest alive monster within radius before a wall.
// Reduces HP; sets alive=false when hp <= 0. Returns true if a monster was hit.
bool apply_hitscan(const Map& map, std::vector<Monster>& monsters, Vec2 from, Vec2 dir,
                   int damage);

// Classic camera-space billboards (colored vertical strip). Skips if wall occludes
// (Collision::hits_wall / raycast cam → monster). FrameBuffer from raycast.hpp.
void draw(Renderer::FrameBuffer& fb, const Camera& camera, const Map& map,
          const std::vector<Monster>& monsters);

}  // namespace AI
