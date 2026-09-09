#pragma once
#include "../camera/camera.hpp"
#include <SDL.h>
struct Input {
  bool key_down(int scancode) const;
  bool key_pressed(int scancode) const;
  Vec2 mouse_delta() const;
  void begin_frame();
  void feed_sdl(const SDL_Event& e);
private:
  bool down_[SDL_NUM_SCANCODES]{};
  bool pressed_[SDL_NUM_SCANCODES]{};
  float mouse_dx_ = 0.0f;
  float mouse_dy_ = 0.0f;
};
