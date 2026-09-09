#include "raycast.hpp"
#include "../collision/collision.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Renderer {
namespace {

constexpr int kTexSize = 64;

enum class TexId : int { Brick = 0, Tech = 1, Door = 2, Count = 3 };

struct Texture {
  uint32_t px[kTexSize * kTexSize];
};

static uint32_t pack_rgb(int r, int g, int b) {
  r = std::clamp(r, 0, 255);
  g = std::clamp(g, 0, 255);
  b = std::clamp(b, 0, 255);
  return 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

static uint32_t shade(uint32_t rgb, float factor) {
  factor = std::clamp(factor, 0.0f, 1.0f);
  const uint32_t r = static_cast<uint32_t>(((rgb >> 16) & 0xFF) * factor);
  const uint32_t g = static_cast<uint32_t>(((rgb >> 8) & 0xFF) * factor);
  const uint32_t b = static_cast<uint32_t>((rgb & 0xFF) * factor);
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static void gen_brick(Texture& t) {
  for (int y = 0; y < kTexSize; ++y) {
    for (int x = 0; x < kTexSize; ++x) {
      const int row = y / 8;
      const int x_off = (row & 1) ? 16 : 0;
      const int bx = (x + x_off) % kTexSize;
      const bool mortar = (y % 8 == 0) || (bx % 16 == 0);
      int r, g, b;
      if (mortar) {
        r = 55; g = 50; b = 48;
      } else {
        // Slight per-brick variation
        const int brick = (row * 7 + (bx / 16) * 13) & 7;
        r = 140 + brick * 6;
        g = 55 + brick * 3;
        b = 45 + brick * 2;
        // Edge darken
        if ((y % 8) == 1 || (bx % 16) == 1) {
          r = static_cast<int>(r * 0.75f);
          g = static_cast<int>(g * 0.75f);
          b = static_cast<int>(b * 0.75f);
        }
      }
      t.px[y * kTexSize + x] = pack_rgb(r, g, b);
    }
  }
}

static void gen_tech(Texture& t) {
  for (int y = 0; y < kTexSize; ++y) {
    for (int x = 0; x < kTexSize; ++x) {
      const bool panel = ((x / 16) ^ (y / 16)) & 1;
      const bool grid = (x % 16 == 0) || (y % 16 == 0) || (x % 16 == 15) || (y % 16 == 15);
      const bool rivet = ((x % 16 == 4 || x % 16 == 11) && (y % 16 == 4 || y % 16 == 11));
      int r, g, b;
      if (rivet) {
        r = 200; g = 210; b = 220;
      } else if (grid) {
        r = 30; g = 36; b = 44;
      } else if (panel) {
        r = 70; g = 90; b = 110;
      } else {
        r = 50; g = 65; b = 80;
      }
      // Subtle scanline
      if ((y & 1) == 0) {
        r = static_cast<int>(r * 0.92f);
        g = static_cast<int>(g * 0.92f);
        b = static_cast<int>(b * 0.92f);
      }
      t.px[y * kTexSize + x] = pack_rgb(r, g, b);
    }
  }
}

static void gen_door(Texture& t) {
  for (int y = 0; y < kTexSize; ++y) {
    for (int x = 0; x < kTexSize; ++x) {
      const bool frame = x < 4 || x >= kTexSize - 4 || y < 4 || y >= kTexSize - 4;
      const bool seam = (x >= 30 && x <= 33);
      const bool handle = (x >= 38 && x <= 44) && (y >= 28 && y <= 36);
      const bool panel = (x >= 8 && x <= 27 && y >= 8 && y <= 55) ||
                        (x >= 36 && x <= 55 && y >= 8 && y <= 55);
      int r, g, b;
      if (handle) {
        r = 220; g = 200; b = 80;
      } else if (frame || seam) {
        r = 40; g = 32; b = 24;
      } else if (panel) {
        // Warm wood / brass door face
        const int band = (y / 8) & 1;
        r = 160 + band * 10;
        g = 110 + band * 6;
        b = 40;
        if ((x + y) % 9 == 0) {
          r = static_cast<int>(r * 0.85f);
          g = static_cast<int>(g * 0.85f);
          b = static_cast<int>(b * 0.85f);
        }
      } else {
        r = 90; g = 70; b = 35;
      }
      t.px[y * kTexSize + x] = pack_rgb(r, g, b);
    }
  }
}

static const Texture* texture_atlas() {
  static Texture atlas[static_cast<int>(TexId::Count)];
  static bool ready = false;
  if (!ready) {
    gen_brick(atlas[static_cast<int>(TexId::Brick)]);
    gen_tech(atlas[static_cast<int>(TexId::Tech)]);
    gen_door(atlas[static_cast<int>(TexId::Door)]);
    ready = true;
  }
  return atlas;
}

static const Texture& tex_at(TexId id) {
  return texture_atlas()[static_cast<int>(id)];
}

static TexId pick_texture(Tile t, int tx, int ty) {
  if (t == Tile::DoorClosed) return TexId::Door;
  // DoorOpen is non-solid (rays pass through) — if ever drawn, tint-like tech
  if (t == Tile::DoorOpen) return TexId::Door;
  // Alternate brick / tech by checkerboard of map cell for variety
  return ((tx + ty) & 1) ? TexId::Tech : TexId::Brick;
}

}  // namespace

void FrameBuffer::resize(int w, int h) {
  width = w;
  height = h;
  pixels.assign(static_cast<size_t>(w * h), 0u);
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

    // Perpendicular wall distance (fisheye correction)
    const float hx = hit.point.x - cam.pos.x;
    const float hy = hit.point.y - cam.pos.y;
    float perp = hx * cam.dir.x + hy * cam.dir.y;
    if (perp < 0.05f) perp = 0.05f;

    int line_h = static_cast<int>(static_cast<float>(h) / perp);
    int draw_start = -line_h / 2 + h / 2;
    int draw_end = line_h / 2 + h / 2;
    if (draw_start < 0) draw_start = 0;
    if (draw_end >= h) draw_end = h - 1;

    const Tile t = map.tile_at(hit.tx, hit.ty);
    const TexId tid = pick_texture(t, hit.tx, hit.ty);
    const Texture& tex = tex_at(tid);

    // Wall X (fraction along the hit face) for texture U
    float wall_x;
    if (hit.side == 0) {
      wall_x = hit.point.y - std::floor(hit.point.y);
    } else {
      wall_x = hit.point.x - std::floor(hit.point.x);
    }
    int tex_x = static_cast<int>(wall_x * static_cast<float>(kTexSize));
    if (tex_x < 0) tex_x = 0;
    if (tex_x >= kTexSize) tex_x = kTexSize - 1;

    // Flip U when looking at opposite faces (classic Wolf look)
    if (hit.side == 0 && ray_dir.x > 0.0f) tex_x = kTexSize - tex_x - 1;
    if (hit.side == 1 && ray_dir.y < 0.0f) tex_x = kTexSize - tex_x - 1;

    // Distance atten + classic darker y-side (EW) walls
    float atten = 1.0f / (1.0f + perp * 0.12f);
    if (hit.side == 1) atten *= 0.70f;
    // DoorClosed slightly warmer/brighter so it reads distinct; DoorOpen not solid
    if (t == Tile::DoorClosed) atten = std::min(1.0f, atten * 1.15f);

    const float step = static_cast<float>(kTexSize) / static_cast<float>(line_h);
    float tex_pos =
        (static_cast<float>(draw_start) - static_cast<float>(h) / 2.0f +
         static_cast<float>(line_h) / 2.0f) *
        step;

    for (int y = draw_start; y <= draw_end; ++y) {
      int tex_y = static_cast<int>(tex_pos) & (kTexSize - 1);
      tex_pos += step;
      const uint32_t sample = tex.px[tex_y * kTexSize + tex_x];
      fb.pixels[static_cast<size_t>(y * w + x)] = shade(sample, atten);
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
