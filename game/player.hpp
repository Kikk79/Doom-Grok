#pragma once
#include "engine/camera/camera.hpp"
#include "engine/collision/collision.hpp"
#include "engine/input/input.hpp"
#include "engine/map/map.hpp"

#include <vector>

// Slice 10 — weapon slots (keys 1/2).
enum class Weapon : int {
  Pistol = 1,
  Shotgun = 2,
};

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
  // Slice 11 — separate magazines (type_id 12 bullets / 13 shells).
  int ammo_bullet = 50;  // pistol
  int ammo_shell = 8;    // shotgun (1 shell per blast)

  // Slice 10 weapons
  Weapon weapon = Weapon::Pistol;
  // Filled by try_fire: unit dirs for each pellet/ray (app runs apply_hitscan per dir).
  std::vector<Vec2> last_shot_dirs;
  int last_shot_damage = 25;  // per ray

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
  // Also handles weapon switch 1/2 (edge via key_pressed).
  void update(const Map& map, const Input& input, float dt);

  // Current magazine for HUD / empty-check.
  int active_ammo() const {
    return weapon == Weapon::Shotgun ? ammo_shell : ammo_bullet;
  }

  // Edge-triggered fire; mouse_clicked from SDL in app layer.
  // Fire: LMB / Space / LCtrl / RCtrl. Consumes 1 bullet or 1 shell; empty → no-op.
  // On success: fills last_shot_dirs + last_shot_damage, plays Fire SFX, muzzle flash.
  // Pistol: 1 ray / 25 dmg. Shotgun: 5 pellets with spread / 10 dmg each.
  bool try_fire(const Map& map, const Input& input, bool mouse_clicked);

  // Slice 3: Use (E / F) — raycast/world_to_tile ahead (~1 unit) → Map::try_open_door.
  bool try_use(Map& map, const Input& input);

  // Slice 5: Melee/projectile damage from AI. Respects i-frames. Returns true if HP changed.
  // Armor absorbs ~half of the hit (up to current armor); remainder goes to health.
  bool take_damage(int amount);

  bool alive() const { return health > 0; }
  bool is_dead() const { return !alive(); }
  bool invulnerable() const { return invuln_t > 0.0f; }

  // Restart helper: pose + vitals + FX. Does not touch map/pickups/monsters (app resets those).
  void reset_vitals(Vec2 spawn_pos, Vec2 spawn_dir);

  // Camera::set_pose follows player pose.
  void sync_camera(Camera& cam) const;

  // Decay flash + i-frame timers (call once per frame with real frame dt).
  void tick_fx(float frame_dt);
};
