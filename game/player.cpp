#include "player.hpp"

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

  auto try_open_at = [&](int tx, int ty) -> bool {
    if (map.tile_at(tx, ty) != Tile::DoorClosed) return false;
    if (!map.try_set_tile(tx, ty, Tile::DoorOpen)) return false;
    std::printf("door opened at (%d,%d)\n", tx, ty);
    return true;
  };

  // Prefer tiles along facing within ~1 unit
  constexpr float kReach = 1.0f;
  for (float d = 0.35f; d <= kReach + 1e-3f; d += 0.35f) {
    const int tx = static_cast<int>(std::floor(pos.x + dir.x * d));
    const int ty = static_cast<int>(std::floor(pos.y + dir.y * d));
    if (try_open_at(tx, ty)) return true;
  }

  // Adjacent tiles with center within ~1 unit of player
  const int ptx = static_cast<int>(std::floor(pos.x));
  const int pty = static_cast<int>(std::floor(pos.y));
  for (int ty = pty - 1; ty <= pty + 1; ++ty) {
    for (int tx = ptx - 1; tx <= ptx + 1; ++tx) {
      const float cx = static_cast<float>(tx) + 0.5f;
      const float cy = static_cast<float>(ty) + 0.5f;
      const float dx = cx - pos.x;
      const float dy = cy - pos.y;
      if (dx * dx + dy * dy > kReach * kReach) continue;
      if (try_open_at(tx, ty)) return true;
    }
  }
  return false;
}

void Player::sync_camera(Camera& cam) const {
  cam.set_pose(pos, dir);
}

void Player::tick_fx(float frame_dt) {
  if (muzzle_flash > 0.0f) {
    muzzle_flash -= frame_dt;
    if (muzzle_flash < 0.0f) muzzle_flash = 0.0f;
  }
}
