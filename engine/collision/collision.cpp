#include "collision.hpp"
#include <algorithm>
#include <cmath>

namespace Collision {
namespace {

bool circle_blocked(const Map& map, float x, float y, float radius) {
  const int min_tx = static_cast<int>(std::floor(x - radius));
  const int max_tx = static_cast<int>(std::floor(x + radius));
  const int min_ty = static_cast<int>(std::floor(y - radius));
  const int max_ty = static_cast<int>(std::floor(y + radius));
  for (int ty = min_ty; ty <= max_ty; ++ty) {
    for (int tx = min_tx; tx <= max_tx; ++tx) {
      if (!map.is_solid(tx, ty)) continue;
      // Closest point on tile AABB [tx,tx+1] x [ty,ty+1] to circle center
      const float cx = std::clamp(x, static_cast<float>(tx), static_cast<float>(tx + 1));
      const float cy = std::clamp(y, static_cast<float>(ty), static_cast<float>(ty + 1));
      const float dx = x - cx;
      const float dy = y - cy;
      if (dx * dx + dy * dy < radius * radius) return true;
    }
  }
  return false;
}

}  // namespace

Vec2 move(const Map& map, Vec2 pos, Vec2 vel, float radius) {
  Vec2 out = pos;
  // Slide on axes independently for simple wall sliding
  const float nx = pos.x + vel.x;
  if (!circle_blocked(map, nx, pos.y, radius)) {
    out.x = nx;
  }
  const float ny = pos.y + vel.y;
  if (!circle_blocked(map, out.x, ny, radius)) {
    out.y = ny;
  }
  return out;
}

bool hits_wall(const Map& map, Vec2 from, Vec2 to) {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 1e-8f) return false;
  const Vec2 dir{dx / dist, dy / dist};
  return raycast(map, from, dir, dist).hit;
}

RayHit raycast(const Map& map, Vec2 from, Vec2 dir, float max_dist) {
  RayHit result{false, max_dist, from, 0, 0};

  float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
  if (len < 1e-8f || max_dist <= 0.0f) return result;
  dir = {dir.x / len, dir.y / len};

  // DDA grid traversal (1 tile = 1.0 world unit)
  int map_x = static_cast<int>(std::floor(from.x));
  int map_y = static_cast<int>(std::floor(from.y));

  const float delta_dist_x = (dir.x == 0.0f) ? 1e30f : std::abs(1.0f / dir.x);
  const float delta_dist_y = (dir.y == 0.0f) ? 1e30f : std::abs(1.0f / dir.y);

  int step_x = 0;
  int step_y = 0;
  float side_dist_x = 0.0f;
  float side_dist_y = 0.0f;

  if (dir.x < 0.0f) {
    step_x = -1;
    side_dist_x = (from.x - map_x) * delta_dist_x;
  } else {
    step_x = 1;
    side_dist_x = (map_x + 1.0f - from.x) * delta_dist_x;
  }
  if (dir.y < 0.0f) {
    step_y = -1;
    side_dist_y = (from.y - map_y) * delta_dist_y;
  } else {
    step_y = 1;
    side_dist_y = (map_y + 1.0f - from.y) * delta_dist_y;
  }

  float traveled = 0.0f;
  int side = 0;
  constexpr int kMaxSteps = 4096;
  for (int i = 0; i < kMaxSteps; ++i) {
    if (side_dist_x < side_dist_y) {
      traveled = side_dist_x;
      side_dist_x += delta_dist_x;
      map_x += step_x;
      side = 0;
    } else {
      traveled = side_dist_y;
      side_dist_y += delta_dist_y;
      map_y += step_y;
      side = 1;
    }
    if (traveled > max_dist) break;
    if (map.is_solid(map_x, map_y)) {
      result.hit = true;
      result.dist = traveled;
      result.point = {from.x + dir.x * traveled, from.y + dir.y * traveled};
      result.tx = map_x;
      result.ty = map_y;
      (void)side;
      return result;
    }
  }
  return result;
}

}  // namespace Collision
