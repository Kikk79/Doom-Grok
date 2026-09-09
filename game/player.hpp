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

  // Vitals
  int health = 100;
  int armor = 0;
  int ammo = 0;  // stub (type_id 11 may bump armor or ammo)

  // Slice 5: brief invulnerability after a hit (seconds remaining).
  float invuln_t = 0.0f;
  static constexpr float kIFrameSec = 0.75f;

  // Hitscan feedback (seconds remaining / last wall hit distance)
  float muzzle_flash = 0.0f;
  float last_hit_dist = -1.0f;
  int last_hit_tx = -1;
  int last_hit_ty = -1;

  void set_pose(Vec2 p, Vec2 d);

  // Mouse look once per rendered frame (not per fixed step).
  void apply_look(const Input& input, float mouse_dx);

  // Fixed-step: turn keys + WASD → Collision::move. No-op if dead.
  void update(const Map& map, const Input& input, float dt);

  // Edge-triggered hitscan; mouse_clicked from SDL in app layer.
  // Fire: LMB / Space / LCtrl / RCtrl.
  bool try_fire(const Map& map, const Input& input, bool mouse_clicked);

  // Slice 3: Use (E / F) — raycast/world_to_tile ahead (~1 unit) → Map::try_open_door.
  bool try_use(Map& map, const Input& input);

  // Slice 5: Melee/projectile damage from AI. Respects i-frames. Returns true if HP changed.
  // Armor absorbs ~half of the hit (up to current armor); remainder goes to health.
  bool take_damage(int amount);

  bool alive() const { return health > 0; }
  bool invulnerable() const { return invuln_t > 0.0f; }

  // Restart helper: pose + vitals + FX. Does not touch map/pickups/monsters (app resets those).
  void reset_vitals(Vec2 spawn_pos, Vec2 spawn_dir);

  // Camera::set_pose follows player pose.
  void sync_camera(Camera& cam) const;

  // Decay flash + i-frame timers (call once per frame with real frame dt).
  void tick_fx(float frame_dt);
};
