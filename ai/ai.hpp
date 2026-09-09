#pragma once
#include "engine/camera/camera.hpp"
#include "engine/map/map.hpp"
#include "engine/renderer/raycast.hpp"

#include <cstdint>
#include <vector>

// Slice 4 — monsters: Idle → Chase on clear LOS; hitscan damage; billboard draw.
namespace AI {

enum class State : uint8_t { Idle = 0, Chase = 1 };

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
};

// Fill defaults from map spawn type_id (1 = Imp-like, 2 / other = tougher/slower).
Monster make_monster(int type_id, Vec2 pos);

// Spawn all Kind::Monster entries from map.spawns().
std::vector<Monster> spawn_from_map(const Map& map);

// Per-frame AI for all alive monsters: LOS → Chase; Chase uses Collision::move.
void update(std::vector<Monster>& monsters, const Map& map, Vec2 player_pos, float dt);

// Hitscan along ray: closest alive monster within radius before a wall.
// Reduces HP; sets alive=false when hp <= 0. Returns true if a monster was hit.
bool apply_hitscan(const Map& map, std::vector<Monster>& monsters, Vec2 from, Vec2 dir,
                   int damage);

// Classic camera-space billboards (colored vertical strip). Skips if wall occludes
// (Collision::hits_wall / raycast cam → monster). FrameBuffer from raycast.hpp.
void draw(Renderer::FrameBuffer& fb, const Camera& camera, const Map& map,
          const std::vector<Monster>& monsters);

}  // namespace AI
