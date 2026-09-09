#include "camera.hpp"
#include <cmath>

void Camera::set_pose(Vec2 p, Vec2 d) {
  pos = p;
  const float len = std::sqrt(d.x * d.x + d.y * d.y);
  if (len > 1e-6f) {
    dir = {d.x / len, d.y / len};
  } else {
    dir = {1.0f, 0.0f};
  }
  // Perpendicular plane vector; FOV ~66° (plane length ~0.66)
  constexpr float plane_scale = 0.66f;
  plane = {-dir.y * plane_scale, dir.x * plane_scale};
}
