#include "ai.hpp"
#include "engine/collision/collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

}  // namespace

int max_hp_for(int type_id) {
  if (type_id == 1) return 40;
  if (type_id == 2) return 60;
  return 30;
}

Monster from_spawn(const EntitySpawn& spawn) {
  Monster m;
  m.type_id = spawn.type_id;
  m.pos = spawn.pos;
  m.hp = max_hp_for(spawn.type_id);
  m.alive = true;
  m.state = State::Idle;
  if (spawn.type_id == 2) {
    m.move_speed = 1.4f;
    m.radius = 0.4f;
  }
  return m;
}

std::vector<Monster> spawn_from_map(const Map& map) {
  std::vector<Monster> out;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Monster) {
      out.push_back(from_spawn(s));
    }
  }
  return out;
}

void tick(Monster& m, const Map& map, Vec2 player_pos, float dt) {
  if (!m.alive || m.hp <= 0) {
    m.alive = false;
    return;
  }

  // LOS: no solid tile between monster and player.
  const bool los = !Collision::hits_wall(map, m.pos, player_pos);
  if (los) {
    m.state = State::Chase;
  } else if (m.state == State::Chase) {
    // Lose aggro when LOS breaks (simple Slice 4 behaviour).
    m.state = State::Idle;
  }

  if (m.state != State::Chase) return;

  Vec2 delta{player_pos.x - m.pos.x, player_pos.y - m.pos.y};
  const float dist = length(delta);
  // Stop short so soft radius vs player can apply; avoid jitter.
  constexpr float kStop = 0.55f;
  if (dist <= kStop) return;

  const Vec2 dir = normalize(delta);
  const Vec2 vel{dir.x * m.move_speed * dt, dir.y * m.move_speed * dt};
  m.pos = Collision::move(map, m.pos, vel, m.radius);
}

void tick_all(std::vector<Monster>& monsters, const Map& map, Vec2 player_pos, float dt) {
  for (auto& m : monsters) tick(m, map, player_pos, dt);
}

void soft_separate(Monster& m, Vec2& player_pos, float player_radius) {
  if (!m.alive) return;
  const float dx = player_pos.x - m.pos.x;
  const float dy = player_pos.y - m.pos.y;
  const float dist = std::sqrt(dx * dx + dy * dy);
  const float min_dist = m.radius + player_radius * 0.65f;
  if (dist >= min_dist) return;
  if (dist < 1e-4f) {
    player_pos.x += min_dist;
    return;
  }
  // Soft: push player only partway (monsters are not hard blockers).
  const float push = (min_dist - dist) * 0.45f;
  player_pos.x += (dx / dist) * push;
  player_pos.y += (dy / dist) * push;
}

bool hitscan(std::vector<Monster>& monsters, Vec2 from, Vec2 dir, float wall_dist,
             int damage) {
  float len = length(dir);
  if (len < 1e-8f || wall_dist <= 0.0f) return false;
  dir = {dir.x / len, dir.y / len};

  int best = -1;
  float best_t = wall_dist;

  for (size_t i = 0; i < monsters.size(); ++i) {
    const Monster& m = monsters[i];
    if (!m.alive) continue;

    // Ray-circle: closest approach along ray to monster center.
    const float fx = m.pos.x - from.x;
    const float fy = m.pos.y - from.y;
    const float t = fx * dir.x + fy * dir.y;  // projection
    if (t < 0.0f || t >= best_t) continue;

    const float px = from.x + dir.x * t;
    const float py = from.y + dir.y * t;
    const float ox = m.pos.x - px;
    const float oy = m.pos.y - py;
    if (ox * ox + oy * oy > m.radius * m.radius) continue;

    best = static_cast<int>(i);
    best_t = t;
  }

  if (best < 0) return false;

  Monster& m = monsters[static_cast<size_t>(best)];
  m.hp -= damage;
  if (m.hp <= 0) {
    m.hp = 0;
    m.alive = false;
    std::printf("monster killed type_id=%d at (%.2f,%.2f)\n",
                m.type_id, m.pos.x, m.pos.y);
  } else {
    std::printf("monster hit type_id=%d hp=%d/%d dist=%.2f\n",
                m.type_id, m.hp, max_hp_for(m.type_id), best_t);
  }
  return true;
}

uint32_t color_for(int type_id) {
  if (type_id == 1) return 0xFFE05040u;  // red-orange
  if (type_id == 2) return 0xFF60C050u;  // green
  return 0xFFC050C0u;                    // magenta default
}

}  // namespace AI
