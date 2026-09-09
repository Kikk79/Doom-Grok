void apply_muzzle_flash(Renderer::FrameBuffer& fb, float strength) {
  if (strength <= 0.0f || fb.width <= 0 || fb.height <= 0) return;
  strength = std::clamp(strength, 0.0f, 1.0f);
  const int w = fb.width;
  const int h = fb.height;
  const uint32_t add = static_cast<uint32_t>(180.0f * strength);
  for (int i = 0; i < w * h; ++i) {
    uint32_t p = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = std::min(255u, ((p >> 16) & 0xFFu) + add);
    uint32_t g = std::min(255u, ((p >> 8) & 0xFFu) + add);
    uint32_t b = std::min(255u, (p & 0xFFu) + add);
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  const int cx = w / 2;
  const int cy = h / 2;
  const uint32_t mark = 0xFFFFFFFFu;
  for (int dx = -2; dx <= 2; ++dx) {
    const int x = cx + dx;
    if (x >= 0 && x < w) fb.pixels[static_cast<size_t>(cy * w + x)] = mark;
  }
  for (int dy = -2; dy <= 2; ++dy) {
    const int y = cy + dy;
    if (y >= 0 && y < h) fb.pixels[static_cast<size_t>(y * w + cx)] = mark;
  }
}

void apply_pickup_effect(Player& player, int type_id) {
  if (type_id == 10) {
    player.health = std::min(200, player.health + 25);
    std::printf("pickup health +25 -> health=%d\n", player.health);
  } else if (type_id == 11) {
    player.armor = std::min(100, player.armor + 10);
    player.ammo += 10;
    std::printf("pickup armor/ammo +10 -> armor=%d ammo=%d\n",
                player.armor, player.ammo);
  } else {
    std::printf("pickup type_id=%d (no effect)\n", type_id);
  }
}

void collect_pickups(Player& player, std::vector<ActivePickup>& pickups) {
  const float r2 = kPickupRadius * kPickupRadius;
  for (size_t i = 0; i < pickups.size();) {
    const float dx = player.pos.x - pickups[i].pos.x;
    const float dy = player.pos.y - pickups[i].pos.y;
    if (dx * dx + dy * dy < r2) {
      apply_pickup_effect(player, pickups[i].type_id);
      Audio::play(Audio::Sfx::Pickup);
      pickups.erase(pickups.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

void draw_hud(Renderer::FrameBuffer& fb, const Player& player) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  auto fill_rect = [&](int x0, int y0, int rw, int rh, uint32_t color) {
    for (int y = y0; y < y0 + rh; ++y) {
      if (y < 0 || y >= h) continue;
      for (int x = x0; x < x0 + rw; ++x) {
        if (x < 0 || x >= w) continue;
        fb.pixels[static_cast<size_t>(y * w + x)] = color;
      }
    }
  };
  constexpr int kBarW = 80;
  constexpr int kBarH = 6;
  const int x0 = 4;
  const int y_hp = h - 12;
  const int y_ar = h - 20;
  fill_rect(x0, y_hp, kBarW, kBarH, 0xFF202020u);
  const int hp_fill = std::clamp(player.health, 0, 200) * (kBarW - 2) / 200;
  const bool blink_off = player.invulnerable() &&
                         (static_cast<int>(player.invuln_t * 10.0f) % 2 == 0);
  const uint32_t hp_col = blink_off ? 0xFF808080u
                                    : (player.health > 25 ? 0xFF40C040u : 0xFFC04040u);
  fill_rect(x0 + 1, y_hp + 1, hp_fill, kBarH - 2, hp_col);
  fill_rect(x0, y_ar, kBarW, 4, 0xFF202020u);
  const int ar_fill = std::clamp(player.armor, 0, 100) * (kBarW - 2) / 100;
  fill_rect(x0 + 1, y_ar + 1, ar_fill, 2, 0xFF4080C0u);
}


void draw_game_over(Renderer::FrameBuffer& fb) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  for (int i = 0; i < w * h; ++i) {
    uint32_t pix = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = ((pix >> 16) & 0xFFu) / 3;
    uint32_t g = ((pix >> 8) & 0xFFu) / 3;
    uint32_t b = (pix & 0xFFu) / 3;
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  const int y0 = h / 2 - 10;
  const int y1 = h / 2 + 10;
  for (int y = y0; y < y1; ++y) {
    if (y < 0 || y >= h) continue;
    for (int x = w / 5; x < (4 * w) / 5; ++x) {
      fb.pixels[static_cast<size_t>(y * w + x)] = 0xFFC01818u;
    }
  }
}

// Slice 6 — distinct from game over: green/gold banner + mild brighten (not darken).
void draw_you_win(Renderer::FrameBuffer& fb) {
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  for (int i = 0; i < w * h; ++i) {
    uint32_t pix = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = std::min(255u, ((pix >> 16) & 0xFFu) + 20u);
    uint32_t g = std::min(255u, ((pix >> 8) & 0xFFu) + 40u);
    uint32_t b = std::min(255u, (pix & 0xFFu) + 10u);
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
  const int y0 = h / 2 - 10;
  const int y1 = h / 2 + 10;
  for (int y = y0; y < y1; ++y) {
    if (y < 0 || y >= h) continue;
    for (int x = w / 5; x < (4 * w) / 5; ++x) {
      fb.pixels[static_cast<size_t>(y * w + x)] = 0xFF18C040u;  // green
    }
  }
  // Gold accent stripe
  const int y_mid = h / 2;
  if (y_mid >= 0 && y_mid < h) {
    for (int x = w / 5; x < (4 * w) / 5; ++x) {
      fb.pixels[static_cast<size_t>(y_mid * w + x)] = 0xFFE0C020u;
    }
  }
}

// Slice 8 — campaign complete (last map cleared): brighter gold banner.
void draw_campaign_complete(Renderer::FrameBuffer& fb) {
  draw_you_win(fb);
  if (fb.width <= 0 || fb.height <= 0) return;
  const int w = fb.width;
  const int h = fb.height;
  const int y0 = h / 2 + 12;
  const int y1 = h / 2 + 18;
  for (int y = y0; y < y1; ++y) {
    if (y < 0 || y >= h) continue;
    for (int x = w / 6; x < (5 * w) / 6; ++x) {
      fb.pixels[static_cast<size_t>(y * w + x)] = 0xFFF0D040u;
    }
  }
}

bool all_monsters_dead(const std::vector<AI::Monster>& monsters) {
  if (monsters.empty()) return false;
  for (const auto& m : monsters) {
    if (m.alive) return false;
  }
  return true;
}

// EntitySpawn Kind::Door is map 'E' — exit marker (optional secondary win trigger).
bool near_exit(const Player& player, const Map& map) {
  const float r2 = kPickupRadius * kPickupRadius;
  for (const auto& s : map.spawns()) {
    if (s.kind != EntitySpawn::Kind::Door) continue;
    const float dx = player.pos.x - s.pos.x;
    const float dy = player.pos.y - s.pos.y;
    if (dx * dx + dy * dy < r2) return true;
  }
  return false;
}

bool check_win(const std::vector<AI::Monster>& monsters, const Player& player,
               const Map& map) {
  if (!player.alive()) return false;
  if (!all_monsters_dead(monsters)) return false;
  // All-dead is enough to win. Exit (E / Kind::Door) after clear is an alternate
  // trigger that yields the same flag (useful if win is deferred; kept for smoke).
  if (near_exit(player, map)) return true;
  return true;
}

void apply_damage_flash(Renderer::FrameBuffer& fb, float invuln_t) {
  if (invuln_t <= 0.0f || fb.width <= 0 || fb.height <= 0) return;
  const float strength = std::clamp(invuln_t / Player::kIFrameSec, 0.0f, 1.0f) * 0.55f;
  const int n = fb.width * fb.height;
  const uint32_t add_r = static_cast<uint32_t>(140.0f * strength);
  for (int i = 0; i < n; ++i) {
    uint32_t pix = fb.pixels[static_cast<size_t>(i)];
    uint32_t r = std::min(255u, ((pix >> 16) & 0xFFu) + add_r);
    uint32_t g = static_cast<uint32_t>(static_cast<float>((pix >> 8) & 0xFFu) * (1.0f - 0.3f * strength));
    uint32_t b = static_cast<uint32_t>(static_cast<float>(pix & 0xFFu) * (1.0f - 0.3f * strength));
    fb.pixels[static_cast<size_t>(i)] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
}

Vec2 find_player_start(const Map& map) {
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::PlayerStart) return s.pos;
  }
  return {2.5f, 2.5f};
}

void rebuild_pickups(const Map& map, std::vector<ActivePickup>& pickups) {
  pickups.clear();
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Item) pickups.push_back({s.pos, s.type_id});
  }
}

bool reload_level(const char* map_path, Map& map, Player& player, Camera& camera,
                  std::vector<AI::Monster>& monsters,
                  std::vector<ActivePickup>& pickups) {
  if (!map.load(map_path)) {
    std::fprintf(stderr, "reload_level: failed to load %s\n", map_path);
    return false;
  }
  player.reset_vitals(find_player_start(map), {1.0f, 0.0f});
  player.sync_camera(camera);
  monsters = AI::spawn_from_map(map);
  rebuild_pickups(map, pickups);
  std::printf("level restarted: monsters=%zu pickups=%zu health=%d\n",
              monsters.size(), pickups.size(), player.health);
  return true;
}
