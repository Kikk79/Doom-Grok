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

  // Slice 3 vitals
  int health = 100;
  int armor = 0;
  int ammo = 0;  // stub (type_id 11 may bump armor or ammo)

  // Hitscan feedback (seconds remaining / last wall hit distance)
  float muzzle_flash = 0.0f;
  float last_hit_dist = -1.0f;
  int last_hit_tx = -1;
  int last_hit_ty = -1;

  // Slice 5: damage / invulnerability
  float invuln = 0.0f;         // seconds remaining (~0.5s after a hit)
  float damage_flash = 0.0f;   // red flash timer
  static constexpr float kInvulnTime = 0.5f;
  static constexpr float kDamageFlashTime = 0.25f;

  void set_pose(Vec2 p, Vec2 d);

  // Mouse look once per rendered frame (not per fixed step).
  void apply_look(const Input& input, float mouse_dx);

  // Fixed-step: turn keys + WASD → Collision::move.
  void update(const Map& map, const Input& input, float dt);

  // Edge-triggered hitscan; mouse_clicked from SDL in app layer.
  // Fire: LMB / Space / LCtrl / RCtrl.
  bool try_fire(const Map& map, const Input& input, bool mouse_clicked);

  // Slice 3: Use (E / F) — raycast/world_to_tile ahead (~1 unit) → Map::try_open_door.
  bool try_use(Map& map, const Input& input);

  // Slice 5: apply damage (armor first, then HP). Honors invuln frames.
  // Returns true if any HP/armor was reduced.
  bool take_damage(int amount);

  bool is_dead() const { return health <= 0; }

  // Reset vitals for level restart (pose set separately).
  void reset_vitals();

  // Camera::set_pose follows player pose.
  void sync_camera(Camera& cam) const;

  // Decay flash / invuln timers (call once per frame with real frame dt).
  void tick_fx(float frame_dt);
};
