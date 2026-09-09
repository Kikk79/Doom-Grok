#include "ai.hpp"
#include "engine/collision/collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace AI {
namespace {

float length(Vec2 v) {
  return std::sqrt(v.x * v.x + v.y * v.y);
}

Vec2 normalize(Vec2 v) {
  const float len = length(v);
  if (len < 1e-6f) return {1.0f, 0.0f};
  return {v.x / len, v.y / len};
}

void apply_type_stats(Monster& m) {
  m.radius = 0.25f;
  if (m.type_id == 1) {
    // Imp-like
    m.max_hp = 30;
    m.speed = 2.0f;
    m.color = 0xFFE05040u;  // reddish
  } else {
    // type_id 2 and default
    m.max_hp = 50;
    m.speed = 1.5f;
    m.color = 0xFF4080C0u;  // greenish/blue
  }
  m.hp = m.max_hp;
}

void tick_one(Monster& m, const Map& map, Vec2 player_pos, float dt) {
  if (!m.alive || m.hp <= 0) {
    m.alive = false;
    return;
  }

  // Clear LOS → Chase (no solid between monster and player).
  const bool los = !Collision::hits_wall(map, m.pos, player_pos);
  if (los) {
    m.state = State::Chase;
  }
  // Stay Idle (or remain Chase once acquired — Slice 4 keeps Chase while LOS holds;
  // lose aggro when LOS breaks so Idle is default again).
  if (!los) {
    m.state = State::Idle;
    return;
  }

  if (m.state != State::Chase) return;

  Vec2 delta{player_pos.x - m.pos.x, player_pos.y - m.pos.y};
  const float dist = length(delta);
  constexpr float kStop = 0.45f;
  if (dist <= kStop) return;

  const Vec2 dir = normalize(delta);
  const Vec2 vel{dir.x * m.speed * dt, dir.y * m.speed * dt};
  m.pos = Collision::move(map, m.pos, vel, m.radius);
}

}  // namespace

Monster make_monster(int type_id, Vec2 pos) {
  Monster m;
  m.type_id = type_id;
  m.pos = pos;
  m.state = State::Idle;
  m.alive = true;
  apply_type_stats(m);
  return m;
}

std::vector<Monster> spawn_from_map(const Map& map) {
  std::vector<Monster> out;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Monster) {
      out.push_back(make_monster(s.type_id, s.pos));
    }
  }
  return out;
}

void update(std::vector<Monster>& monsters, const Map& map, Vec2 player_pos, float dt) {
  for (auto& m : monsters) tick_one(m, map, player_pos, dt);
}

bool apply_hitscan(const Map& map, std::vector<Monster>& monsters, Vec2 from, Vec2 dir,
                   int damage) {
  float len = length(dir);
  if (len < 1e-8f) return false;
  dir = {dir.x / len, dir.y / len};

  constexpr float kMaxDist = 64.0f;
  const Collision::RayHit wall = Collision::raycast(map, from, dir, kMaxDist);
  const float wall_dist = wall.hit ? wall.dist : kMaxDist;
  if (wall_dist <= 0.0f) return false;

  int best = -1;
  float best_t = wall_dist;

  for (size_t i = 0; i < monsters.size(); ++i) {
    const Monster& m = monsters[i];
    if (!m.alive) continue;

    // Ray-circle: closest approach along ray to monster center.
    const float fx = m.pos.x - from.x;
    const float fy = m.pos.y - from.y;
    const float t = fx * dir.x + fy * dir.y;
    if (t < 0.0f || t >= best_t) continue;

    const float px = from.x + dir.x * t;
    const float py = from.y + dir.y * t;
    const float ox = m.pos.x - px;
    const float oy = m.pos.y - py;
    const float r = m.radius;
    if (ox * ox + oy * oy > r * r) continue;

    best = static_cast<int>(i);
    best_t = t;
  }

  if (best < 0) return false;

  Monster& m = monsters[static_cast<size_t>(best)];
  m.hp -= damage;
  if (m.hp <= 0) {
    m.hp = 0;
    m.alive = false;
  }
  return true;
}

void draw(Renderer::FrameBuffer& fb, const Camera& camera, const Map& map,
          const std::vector<Monster>& monsters) {
  if (fb.width <= 0 || fb.height <= 0 || fb.pixels.empty()) return;

  const int w = fb.width;
  const int h = fb.height;
  const float det = camera.plane.x * camera.dir.y - camera.dir.x * camera.plane.y;
  if (std::fabs(det) < 1e-8f) return;
  const float inv_det = 1.0f / det;

  for (const auto& m : monsters) {
    if (!m.alive) continue;

    // Occlusion: wall between camera and monster → skip.
    if (Collision::hits_wall(map, camera.pos, m.pos)) continue;

    const float rel_x = m.pos.x - camera.pos.x;
    const float rel_y = m.pos.y - camera.pos.y;

    // Camera-space transform (classic Wolf3D sprite projection).
    const float transform_x = inv_det * (camera.dir.y * rel_x - camera.dir.x * rel_y);
    const float transform_y = inv_det * (-camera.plane.y * rel_x + camera.plane.x * rel_y);
    if (transform_y <= 0.05f) continue;  // behind camera

    // Extra occlusion check: wall closer than sprite depth along cam→monster.
    const float world_dist = length(Vec2{rel_x, rel_y});
    const Collision::RayHit wall =
        Collision::raycast(map, camera.pos, normalize(Vec2{rel_x, rel_y}), world_dist + 0.1f);
    if (wall.hit && wall.dist + 0.05f < world_dist) continue;

    const int screen_x =
        static_cast<int>((static_cast<float>(w) / 2.0f) * (1.0f + transform_x / transform_y));
    // Height from depth; width from monster radius in camera space.
    const int sprite_h = std::abs(static_cast<int>(static_cast<float>(h) / transform_y));
    const float aspect = m.radius * 2.0f;  // full width in world units ≈ diameter
    const int sprite_w =
        std::abs(static_cast<int>(static_cast<float>(h) * aspect / transform_y));
    if (sprite_h <= 0 || sprite_w <= 0) continue;

    int draw_start_y = -sprite_h / 2 + h / 2;
    int draw_end_y = sprite_h / 2 + h / 2;
    if (draw_start_y < 0) draw_start_y = 0;
    if (draw_end_y >= h) draw_end_y = h - 1;

    int draw_start_x = -sprite_w / 2 + screen_x;
    int draw_end_x = sprite_w / 2 + screen_x;
    if (draw_start_x < 0) draw_start_x = 0;
    if (draw_end_x >= w) draw_end_x = w - 1;

    const uint32_t col = m.color;
    for (int x = draw_start_x; x <= draw_end_x; ++x) {
      // Simple vertical strip fill (no texture).
      for (int y = draw_start_y; y <= draw_end_y; ++y) {
        fb.pixels[static_cast<size_t>(y * w + x)] = col;
      }
    }
  }
}

}  // namespace AI
