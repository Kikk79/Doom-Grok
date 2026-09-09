#pragma once
// INTERNAL — not part of the public game/ai API.
#include "../camera/camera.hpp"
#include "../map/map.hpp"
#include <SDL.h>
#include <vector>

namespace Renderer {

struct FrameBuffer {
  int width = 0;
  int height = 0;
  std::vector<uint32_t> pixels;  // ARGB8888
  void resize(int w, int h);
};

// Classic Wolfenstein-style column raycast into an ARGB buffer.
void raycast_view(const Map& map, const Camera& cam, FrameBuffer& fb);

// Blit framebuffer to an SDL texture (ARGB8888) and present.
void present(SDL_Renderer* renderer, SDL_Texture* texture, const FrameBuffer& fb);

}  // namespace Renderer
