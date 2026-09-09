#pragma once
#include "../camera/camera.hpp"
#include "../map/map.hpp"
namespace Collision {
  Vec2 move(const Map& map, Vec2 pos, Vec2 vel, float radius);
  bool hits_wall(const Map& map, Vec2 from, Vec2 to);
  struct RayHit { bool hit; float dist; Vec2 point; int tx, ty; };
  RayHit raycast(const Map& map, Vec2 from, Vec2 dir, float max_dist);
}
