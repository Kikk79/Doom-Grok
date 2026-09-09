#include "engine/camera/camera.hpp"
#include "engine/collision/collision.hpp"
#include "engine/input/input.hpp"
#include "engine/map/map.hpp"
#include "engine/renderer/raycast.hpp"
#include "engine/timing/timing.hpp"
#include "game/player.hpp"
#include "ai/ai.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr int kWinW = 960;
constexpr int kWinH = 600;
constexpr int kFbW = 320;
constexpr int kFbH = 200;
constexpr float kPickupRadius = 0.5f;

struct ActivePickup {
  Vec2 pos;
  int type_id;
};

// Slice 8 — fixed campaign order (relative names; resolved at runtime).
constexpr const char* kCampaignLevels[] = {
  "levels/demo.map",
  "levels/e1m2.map",
};
constexpr int kCampaignCount = 2;

bool file_readable(const std::string& p) {
  FILE* f = std::fopen(p.c_str(), "rb");
  if (!f) return false;
  std::fclose(f);
  return true;
}

// Resolve a levels/... relative path via DOOM_GROK_ROOT / cwd / ../.
std::string resolve_map_path(const char* relative) {
  const char* env = std::getenv("DOOM_GROK_ROOT");
  if (env && env[0]) {
    std::string p = std::string(env) + "/" + relative;
    if (file_readable(p)) return p;
  }
  const char* prefixes[] = {"", "./", "../"};
  for (const char* pre : prefixes) {
    std::string p = std::string(pre) + relative;
    if (file_readable(p)) return p;
  }
  // Basename fallback (binary-next-to-levels after CMake copy).
  const char* slash = std::strrchr(relative, '/');
  const char* base = slash ? slash + 1 : relative;
  std::string near = std::string("levels/") + base;
  if (file_readable(near)) return near;
  return relative;
}

std::string find_map_path() {
  return resolve_map_path(kCampaignLevels[0]);
}

std::string map_basename(const std::string& path) {
  const auto slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

int campaign_index_for(const std::string& path) {
  const std::string base = map_basename(path);
  for (int i = 0; i < kCampaignCount; ++i) {
    if (base == map_basename(kCampaignLevels[i])) return i;
  }
  return -1;
}

// Empty string => current is last map (campaign complete).
std::string next_campaign_path(const std::string& current) {
  const int idx = campaign_index_for(current);
  if (idx < 0 || idx + 1 >= kCampaignCount) return {};
  return resolve_map_path(kCampaignLevels[idx + 1]);
}

std::string first_campaign_path() {
  return resolve_map_path(kCampaignLevels[0]);
}
