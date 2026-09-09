#include "player.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

void rotate_dir(Vec2& dir, float angle) {
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  const Vec2 d = dir;
  dir = {d.x * c - d.y * s, d.x * s + d.y * c};
  const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
  if (len > 1e-6f) {
    dir.x /= len;
    dir.y /= len;
  } else {
    dir = {1.0f, 0.0f};
  }
}

}  // namespace

void Player::set_pose(Vec2 p, Vec2 d) {
  pos = p;
  const float len = std::sqrt(d.x * d.x + d.y * d.y);
  if (len > 1e-6f) {
    dir = {d.x / len, d.y / len};
  } else {
    dir = {1.0f, 0.0f};
  }
}

void Player::apply_look(const Input& /*input*/, float mouse_dx) {
  if (mouse_dx != 0.0f) {
    rotate_dir(dir, mouse_dx * mouse_sens);
  }
}

void Player::update(const Map& map, const Input& input, float dt) {
  float turn = 0.0f;
  if (input.key_down(SDL_SCANCODE_LEFT)) turn -= turn_speed * dt;
  if (input.key_down(SDL_SCANCODE_RIGHT)) turn += turn_speed * dt;
  if (turn != 0.0f) rotate_dir(dir, turn);

  Vec2 wish{0.0f, 0.0f};
  if (input.key_down(SDL_SCANCODE_W) || input.key_down(SDL_SCANCODE_UP)) {
    wish.x += dir.x;
    wish.y += dir.y;
  }
  if (input.key_down(SDL_SCANCODE_S) || input.key_down(SDL_SCANCODE_DOWN)) {
    wish.x -= dir.x;
    wish.y -= dir.y;
  }
  if (input.key_down(SDL_SCANCODE_A)) {
    wish.x -= dir.y;
    wish.y += dir.x;
  }
  if (input.key_down(SDL_SCANCODE_D)) {
    wish.x += dir.y;
    wish.y -= dir.x;
  }

  const float wlen = std::sqrt(wish.x * wish.x + wish.y * wish.y);
  Vec2 vel{0.0f, 0.0f};
  if (wlen > 1e-6f) {
    vel.x = (wish.x / wlen) * move_speed * dt;
    vel.y = (wish.y / wlen) * move_speed * dt;
  }
  pos = Collision::move(map, pos, vel, radius);
}

bool Player::try_fire(const Map& map, const Input& input, bool mouse_clicked) {
  const bool key_fire =
      input.key_pressed(SDL_SCANCODE_SPACE) ||
      input.key_pressed(SDL_SCANCODE_LCTRL) ||
      input.key_pressed(SDL_SCANCODE_RCTRL);
  if (!mouse_clicked && !key_fire) return false;

  constexpr float kMaxDist = 64.0f;
  const Collision::RayHit hit = Collision::raycast(map, pos, dir, kMaxDist);
  muzzle_flash = 0.08f;  // brief screen flash
  if (hit.hit) {
    last_hit_dist = hit.dist;
    last_hit_tx = hit.tx;
    last_hit_ty = hit.ty;
    std::printf("hitscan wall hit dist=%.2f tile=(%d,%d)\n",
                hit.dist, hit.tx, hit.ty);
  } else {
    last_hit_dist = -1.0f;
    last_hit_tx = -1;
    last_hit_ty = -1;
    std::printf("hitscan miss\n");
  }
  return true;
}

bool Player::try_use(Map& map, const Input& input) {
  if (!input.key_pressed(SDL_SCANCODE_E) && !input.key_pressed(SDL_SCANCODE_F)) {
    return false;
  }

  // Use: look ~1 unit ahead (ray / world_to_tile), then try_open_door.
  constexpr float kReach = 1.0f;
  const Collision::RayHit hit = Collision::raycast(map, pos, dir, kReach);
  int tx = -1, ty = -1;
  if (hit.hit) {
    tx = hit.tx;
    ty = hit.ty;
  } else {
    const Vec2 ahead{pos.x + dir.x * kReach, pos.y + dir.y * kReach};
    if (!map.world_to_tile(ahead, tx, ty)) return false;
  }

  if (!map.try_open_door(tx, ty)) return false;
  std::printf("door opened at (%d,%d)\n", tx, ty);
  return true;
}

bool Player::take_damage(int amount) {
  if (amount <= 0) return false;
  if (invuln > 0.0f) return false;
  if (health <= 0) return false;

  int remaining = amount;
  if (armor > 0) {
    const int absorbed = std::min(armor, remaining);
    armor -= absorbed;
    remaining -= absorbed;
  }
  if (remaining > 0) {
    health -= remaining;
    if (health < 0) health = 0;
  }

  invuln = kInvulnTime;
  damage_flash = kDamageFlashTime;
  std::printf("player damaged %d -> health=%d armor=%d invuln=%.2f\n",
              amount, health, armor, invuln);
  return true;
}

void Player::reset_vitals() {
  health = 100;
  armor = 0;
  ammo = 0;
  invuln = 0.0f;
  damage_flash = 0.0f;
  muzzle_flash = 0.0f;
  last_hit_dist = -1.0f;
  last_hit_tx = -1;
  last_hit_ty = -1;
}

void Player::sync_camera(Camera& cam) const {
  cam.set_pose(pos, dir);
}

void Player::tick_fx(float frame_dt) {
  if (muzzle_flash > 0.0f) {
    muzzle_flash -= frame_dt;
    if (muzzle_flash < 0.0f) muzzle_flash = 0.0f;
  }
  if (invuln > 0.0f) {
    invuln -= frame_dt;
    if (invuln < 0.0f) invuln = 0.0f;
  }
  if (damage_flash > 0.0f) {
    damage_flash -= frame_dt;
    if (damage_flash < 0.0f) damage_flash = 0.0f;
  }
}
