#pragma once
// INTERNAL — not part of the public game/ai API.
#include "../camera/camera.hpp"
#include "../map/map.hpp"
#include <SDL.h>
#include <cstdint>
#include <vector>

namespace Renderer {

struct FrameBuffer {
  int width = 0;
  int height = 0;
  std::vector<uint32_t> pixels;  // ARGB8888
  std::vector<float> depth;      // per-column wall depth (Slice 4 sprites)
  void resize(int w, int h);
};

struct Billboard {
  float x = 0.0f;
  float y = 0.0f;
  uint32_t color = 0xFFFFFFFFu;
  float radius = 0.35f;  // half-width in world units
};

// Classic Wolfenstein-style column raycast into an ARGB buffer.
void raycast_view(const Map& map, const Camera& cam, FrameBuffer& fb);

// Slice 4: simple colored billboards (filled columns) with depth test.
void draw_billboards(const Camera& cam, FrameBuffer& fb,
                     const std::vector<Billboard>& sprites);

// Blit framebuffer to an SDL texture (ARGB8888) and present.
void present(SDL_Renderer* renderer, SDL_Texture* texture, const FrameBuffer& fb);

}  // namespace Renderer
