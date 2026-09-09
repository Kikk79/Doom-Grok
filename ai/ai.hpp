#pragma once
#include "engine/camera/camera.hpp"
#include "engine/map/map.hpp"
#include <cstdint>
#include <vector>

// Slice 4 — first monsters / simple AI (Idle → Chase on LOS).
namespace AI {

enum class State : uint8_t { Idle = 0, Chase = 1 };

struct Monster {
  int type_id = 0;
  Vec2 pos{0.0f, 0.0f};
  float radius = 0.35f;
  float move_speed = 1.8f;
  int hp = 0;
  bool alive = true;
  State state = State::Idle;
};

// HP by type_id (map M … type_id).
int max_hp_for(int type_id);

// Build a living monster from a Map monster spawn.
Monster from_spawn(const EntitySpawn& spawn);

// Spawn all Kind::Monster entries from map.spawns().
std::vector<Monster> spawn_from_map(const Map& map);

// Idle → Chase when clear LOS to player (Collision::hits_wall);
// in Chase, Collision::move toward player pose (not Camera).
void tick(Monster& m, const Map& map, Vec2 player_pos, float dt);
void tick_all(std::vector<Monster>& monsters, const Map& map, Vec2 player_pos, float dt);

// Soft radius separation so monsters do not act as hard walls.
void soft_separate(Monster& m, Vec2& player_pos, float player_radius);

// Hitscan along ray: damage nearest living monster closer than wall_dist
// whose body intersects the ray. Returns true if a monster was hit.
// Deactivates (alive=false) on death.
bool hitscan(std::vector<Monster>& monsters, Vec2 from, Vec2 dir, float wall_dist,
             int damage = 20);

// Billboard color for living monsters (renderer).
uint32_t color_for(int type_id);

}  // namespace AI
