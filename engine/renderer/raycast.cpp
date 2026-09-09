#include "raycast.hpp"
#include "../collision/collision.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Renderer {

void FrameBuffer::resize(int w, int h) {
  width = w;
  height = h;
  pixels.assign(static_cast<size_t>(w * h), 0u);
}

static uint32_t shade(uint32_t rgb, float factor) {
  factor = std::clamp(factor, 0.0f, 1.0f);
  const uint32_t r = static_cast<uint32_t>(((rgb >> 16) & 0xFF) * factor);
  const uint32_t g = static_cast<uint32_t>(((rgb >> 8) & 0xFF) * factor);
  const uint32_t b = static_cast<uint32_t>((rgb & 0xFF) * factor);
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void raycast_view(const Map& map, const Camera& cam, FrameBuffer& fb) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;

  // Ceiling / floor
  const uint32_t ceiling = 0xFF303848u;
  const uint32_t floor_c = 0xFF4A4038u;
  for (int y = 0; y < h; ++y) {
    const uint32_t c = (y < h / 2) ? ceiling : floor_c;
    uint32_t* row = &fb.pixels[static_cast<size_t>(y * w)];
    for (int x = 0; x < w; ++x) row[x] = c;
  }

  for (int x = 0; x < w; ++x) {
    const float camera_x = 2.0f * static_cast<float>(x) / static_cast<float>(w) - 1.0f;
    const Vec2 ray_dir{
      cam.dir.x + cam.plane.x * camera_x,
      cam.dir.y + cam.plane.y * camera_x
    };

    auto hit = Collision::raycast(map, cam.pos, ray_dir, 64.0f);
    if (!hit.hit) continue;

    // Perpendicular wall distance for fisheye correction
    float perp = hit.dist;
    const float rdx = ray_dir.x;
    const float rdy = ray_dir.y;
    // Correct using camera direction projection
    const float inv = rdx * cam.dir.x + rdy * cam.dir.y;
    if (std::abs(inv) > 1e-6f) {
      // Use euclidean hit distance * cos(angle) ≈ dot(ray, dir) * |ray| but ray is unit in DDA after norm
      // Collision::raycast normalizes dir; camera ray_dir may not be unit — recompute properly:
    }
    // Re-run distance as |hit.point - cam.pos| projected onto camera dir
    const float hx = hit.point.x - cam.pos.x;
    const float hy = hit.point.y - cam.pos.y;
    perp = hx * cam.dir.x + hy * cam.dir.y;
    if (perp < 0.05f) perp = 0.05f;

    int line_h = static_cast<int>(static_cast<float>(h) / perp);
    int draw_start = -line_h / 2 + h / 2;
    int draw_end = line_h / 2 + h / 2;
    if (draw_start < 0) draw_start = 0;
    if (draw_end >= h) draw_end = h - 1;

    // Color by tile type + side shading
    uint32_t base = 0xFF888888u;
    const Tile t = map.tile_at(hit.tx, hit.ty);
    if (t == Tile::Wall) {
      base = ((hit.tx + hit.ty) & 1) ? 0xFFB05050u : 0xFF906060u;
    } else if (t == Tile::DoorClosed) {
      base = 0xFFC0A040u;
    }

    // Darken with distance
    float atten = 1.0f / (1.0f + perp * 0.12f);
    // Slight side variation from which axis we hit
    const bool side_x = std::abs(hit.point.x - std::floor(hit.point.x + 1e-4f)) < 1e-3f
                     || std::abs(hit.point.x - std::ceil(hit.point.x - 1e-4f)) < 1e-3f;
    if (side_x) atten *= 0.75f;

    const uint32_t col = shade(base, atten);
    for (int y = draw_start; y <= draw_end; ++y) {
      fb.pixels[static_cast<size_t>(y * w + x)] = col;
    }
  }
}

void present(SDL_Renderer* renderer, SDL_Texture* texture, const FrameBuffer& fb) {
  void* pixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0) return;
  auto* dst = static_cast<uint8_t*>(pixels);
  const int row_bytes = fb.width * 4;
  for (int y = 0; y < fb.height; ++y) {
    std::memcpy(dst + y * pitch,
                fb.pixels.data() + static_cast<size_t>(y * fb.width),
                static_cast<size_t>(row_bytes));
  }
  SDL_UnlockTexture(texture);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

}  // namespace Renderer
