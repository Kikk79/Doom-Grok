#pragma once
// AI stubs — monsters / pathing to be filled in later.
namespace AI {
  struct Agent {
    int type_id = 0;
    float x = 0.0f;
    float y = 0.0f;
    bool alive = true;
  };
  inline void tick_stub(Agent&, float /*dt*/) {
    // no-op placeholder
  }
}
