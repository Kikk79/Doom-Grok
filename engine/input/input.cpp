#include "input.hpp"
#include <cstring>

bool Input::key_down(int scancode) const {
  if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) return false;
  return down_[scancode];
}

bool Input::key_pressed(int scancode) const {
  if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) return false;
  return pressed_[scancode];
}

Vec2 Input::mouse_delta() const {
  return {mouse_dx_, mouse_dy_};
}

void Input::begin_frame() {
  std::memset(pressed_, 0, sizeof(pressed_));
  mouse_dx_ = 0.0f;
  mouse_dy_ = 0.0f;
}

void Input::feed_sdl(const SDL_Event& e) {
  if (e.type == SDL_KEYDOWN && !e.key.repeat) {
    const int sc = e.key.keysym.scancode;
    if (sc >= 0 && sc < SDL_NUM_SCANCODES) {
      if (!down_[sc]) pressed_[sc] = true;
      down_[sc] = true;
    }
  } else if (e.type == SDL_KEYUP) {
    const int sc = e.key.keysym.scancode;
    if (sc >= 0 && sc < SDL_NUM_SCANCODES) {
      down_[sc] = false;
    }
  } else if (e.type == SDL_MOUSEMOTION) {
    mouse_dx_ += static_cast<float>(e.motion.xrel);
    mouse_dy_ += static_cast<float>(e.motion.yrel);
  }
}
