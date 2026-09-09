#pragma once
#include "engine/camera/camera.hpp"
#include "engine/collision/collision.hpp"
#include "engine/input/input.hpp"
#include "engine/map/map.hpp"

// Canonical gameplay pose lives here; Camera follows via sync_camera().
struct Player {
  Vec2 pos{0.0f, 0.0f};
  Vec2 dir{1.0f, 0.0f};

  float radius = 0.25f;
  float move_speed = 3.5f;   // units / second
  float turn_speed = 2.5f;   // radians / second
  float mouse_sens = 0.003f;

  // Hitscan feedback (seconds remaining / last wall hit distance)
  float muzzle_flash = 0.0f;
  float last_hit_dist = -1.0f;
  int last_hit_tx = -1;
  int last_hit_ty = -1;

  void set_pose(Vec2 p, Vec2 d);

  // Mouse look once per rendered frame (not per fixed step).
  void apply_look(const Input& input, float mouse_dx);

  // Fixed-step: turn keys + WASD → Collision::move.
  void update(const Map& map, const Input& input, float dt);

  // Edge-triggered hitscan; mouse_clicked from SDL in app layer.
  // Fire: LMB / Space / LCtrl / RCtrl.
  bool try_fire(const Map& map, const Input& input, bool mouse_clicked);

  // Camera::set_pose follows player pose.
  void sync_camera(Camera& cam) const;

  // Decay flash timer (call once per frame with real frame dt).
  void tick_fx(float frame_dt);
};
