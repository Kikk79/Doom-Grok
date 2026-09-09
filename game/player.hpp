#pragma once
#include "engine/camera/camera.hpp"

// Placeholder player state — expand later (health, weapons, inventory).
struct Player {
  Camera camera;
  float radius = 0.25f;
  float move_speed = 3.5f;   // units / second
  float turn_speed = 2.5f;   // radians / second
  float mouse_sens = 0.003f;
};
