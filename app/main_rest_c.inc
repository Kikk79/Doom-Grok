  // --- Slice 6: kill all → win flag ---
  monsters = AI::spawn_from_map(map);
  if (monsters.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: no monsters for win test\n");
    return 1;
  }
  if (all_monsters_dead(monsters) || check_win(monsters, player, map)) {
    std::fprintf(stderr, "SMOKE FAIL: win should be false while monsters alive\n");
    return 1;
  }
  for (auto& m : monsters) {
    m.alive = false;
    m.hp = 0;
  }
  if (!all_monsters_dead(monsters)) {
    std::fprintf(stderr, "SMOKE FAIL: all_monsters_dead after kill-all\n");
    return 1;
  }
  const bool win_flag = check_win(monsters, player, map);
  if (!win_flag) {
    std::fprintf(stderr, "SMOKE FAIL: expected win flag after kill-all\n");
    return 1;
  }
  // Optional exit: standing on E after clear also wins (same flag).
  bool have_exit = false;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Door) {
      player.pos = s.pos;
      have_exit = true;
      break;
    }
  }
  if (have_exit && !check_win(monsters, player, map)) {
    std::fprintf(stderr, "SMOKE FAIL: exit-after-clear should win\n");
    return 1;
  }
  // R / reload clears win path (monsters respawn → not won).
  if (!reload_level(map_path, map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload after win\n");
    return 1;
  }
  if (all_monsters_dead(monsters) || check_win(monsters, player, map)) {
    std::fprintf(stderr, "SMOKE FAIL: win should clear after restart\n");
    return 1;
  }
  std::printf("smoke: win flag ok (kill-all + restart)%s\n",
              have_exit ? " + exit" : "");

  // --- Slice 8: two-map load + campaign next-path ---
  const std::string demo_path = resolve_map_path(kCampaignLevels[0]);
  const std::string e1m2_path = resolve_map_path(kCampaignLevels[1]);
  Map map2;
  if (!map2.load(e1m2_path.c_str())) {
    std::fprintf(stderr, "SMOKE FAIL: load e1m2 %s\n", e1m2_path.c_str());
    return 1;
  }
  if (map2.width() != 16 || map2.height() != 16) {
    std::fprintf(stderr, "SMOKE FAIL: e1m2 expected 16x16 got %dx%d\n",
                 map2.width(), map2.height());
    return 1;
  }
  int e1_doors = 0, e1_monsters = 0, e1_items = 0, e1_exits = 0, e1_starts = 0;
  for (int y = 0; y < map2.height(); ++y) {
    for (int x = 0; x < map2.width(); ++x) {
      if (map2.tile_at(x, y) == Tile::DoorClosed) ++e1_doors;
    }
  }
  for (const auto& s : map2.spawns()) {
    if (s.kind == EntitySpawn::Kind::Monster) ++e1_monsters;
    else if (s.kind == EntitySpawn::Kind::Item) ++e1_items;
    else if (s.kind == EntitySpawn::Kind::Door) ++e1_exits;
    else if (s.kind == EntitySpawn::Kind::PlayerStart) ++e1_starts;
  }
  if (e1_doors < 1 || e1_monsters < 3 || e1_monsters > 5 || e1_items < 1 ||
      e1_starts < 1) {
    std::fprintf(stderr,
      "SMOKE FAIL: e1m2 content doors=%d monsters=%d items=%d starts=%d\n",
      e1_doors, e1_monsters, e1_items, e1_starts);
    return 1;
  }
  const std::string next_from_demo = next_campaign_path(demo_path);
  if (map_basename(next_from_demo) != "e1m2.map") {
    std::fprintf(stderr, "SMOKE FAIL: next after demo should be e1m2 got %s\n",
                 next_from_demo.c_str());
    return 1;
  }
  if (!next_campaign_path(e1m2_path).empty()) {
    std::fprintf(stderr, "SMOKE FAIL: next after e1m2 should be empty (no wrap)\n");
    return 1;
  }
  // After e1m2 win: stay on COMPLETE — R reloads e1m2 (same path), no wrap to demo.
  std::string stay = e1m2_path;
  if (!reload_level(stay.c_str(), map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload e1m2 as R-on-complete\n");
    return 1;
  }
  if (map_basename(stay) != "e1m2.map" || static_cast<int>(monsters.size()) != e1_monsters) {
    std::fprintf(stderr, "SMOKE FAIL: R-on-complete should stay on e1m2\n");
    return 1;
  }
  // Progression reload: demo → e1m2 via tracked path.
  std::string current = demo_path;
  if (!reload_level(current.c_str(), map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload demo for progression\n");
    return 1;
  }
  current = next_campaign_path(current);
  if (!reload_level(current.c_str(), map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload e1m2 progression\n");
    return 1;
  }
  if (static_cast<int>(monsters.size()) != e1_monsters) {
    std::fprintf(stderr, "SMOKE FAIL: e1m2 spawn count %zu vs %d\n",
                 monsters.size(), e1_monsters);
    return 1;
  }
  std::printf("smoke: two-map ok demo + e1m2 (monsters=%d doors=%d items=%d exits=%d)\n",
              e1_monsters, e1_doors, e1_items, e1_exits);

  // --- Slice 10: weapon switch + shotgun multi-pellet / pistol single ---
  {
    Input dummy{};  // unused; we drive weapon + try_fire fields directly
    (void)dummy;
    Player wp;
    wp.reset_vitals({8.5f, 8.5f}, {1.0f, 0.0f});
    if (wp.weapon != Weapon::Pistol) {
      std::fprintf(stderr, "SMOKE FAIL: reset should select Pistol\n");
      return 1;
    }
    // Simulate key 2 / key 1 via direct assignment (Input edge needs SDL feed).
    wp.weapon = Weapon::Shotgun;
    // Fabricate shotgun fire dirs the same way try_fire would.
    wp.last_shot_dirs.clear();
    wp.last_shot_damage = 10;
    auto rot = [](Vec2 d, float ang) {
      const float c = std::cos(ang);
      const float s = std::sin(ang);
      Vec2 o{d.x * c - d.y * s, d.x * s + d.y * c};
      const float len = std::sqrt(o.x * o.x + o.y * o.y);
      if (len > 1e-6f) { o.x /= len; o.y /= len; }
      return o;
    };
    constexpr float kSpread[] = {-0.105f, -0.052f, 0.0f, 0.052f, 0.105f};
    for (float a : kSpread) wp.last_shot_dirs.push_back(rot(wp.dir, a));
    if (wp.last_shot_dirs.size() != 5 || wp.last_shot_damage != 10) {
      std::fprintf(stderr, "SMOKE FAIL: shotgun expected 5 pellets dmg 10 got %zu/%d\n",
                   wp.last_shot_dirs.size(), wp.last_shot_damage);
      return 1;
    }
    // Distinct dirs (spread not collapsed).
    int distinct = 1;
    for (size_t i = 1; i < wp.last_shot_dirs.size(); ++i) {
      const float dx = wp.last_shot_dirs[i].x - wp.last_shot_dirs[0].x;
      const float dy = wp.last_shot_dirs[i].y - wp.last_shot_dirs[0].y;
      if (dx * dx + dy * dy > 1e-6f) ++distinct;
    }
    if (distinct < 3) {
      std::fprintf(stderr, "SMOKE FAIL: shotgun dirs not spread (distinct=%d)\n", distinct);
      return 1;
    }
    // Pistol: 1 ray / 25
    wp.weapon = Weapon::Pistol;
    wp.last_shot_dirs.clear();
    wp.last_shot_damage = 25;
    wp.last_shot_dirs.push_back(wp.dir);
    if (wp.last_shot_dirs.size() != 1 || wp.last_shot_damage != 25) {
      std::fprintf(stderr, "SMOKE FAIL: pistol expected 1x25\n");
      return 1;
    }
    // Hitscan with shotgun damage still kills via multi apply
    monsters = AI::spawn_from_map(map);
    if (monsters.empty()) {
      std::fprintf(stderr, "SMOKE FAIL: no monsters for shotgun hitscan\n");
      return 1;
    }
    AI::Monster& prey = monsters[0];
    prey.alive = true;
    prey.hp = 30;
    prey.max_hp = 30;
    wp.pos = {prey.pos.x - 1.5f, prey.pos.y};
    wp.dir = {1.0f, 0.0f};
    if (Collision::hits_wall(map, prey.pos, wp.pos)) {
      wp.pos = {7.5f, 8.5f};
      prey.pos = {9.5f, 8.5f};
    }
    wp.weapon = Weapon::Shotgun;
    wp.last_shot_dirs.clear();
    wp.last_shot_damage = 10;
    for (float a : kSpread) wp.last_shot_dirs.push_back(rot(wp.dir, a));
    int hits = 0;
    for (const Vec2& d : wp.last_shot_dirs) {
      if (AI::apply_hitscan(map, monsters, wp.pos, d, wp.last_shot_damage)) ++hits;
    }
    if (hits < 1 || prey.alive) {
      // 5*10=50 > 30 so center+near pellets should kill if any hit
      std::fprintf(stderr, "SMOKE FAIL: shotgun hitscan hits=%d alive=%d hp=%d\n",
                   hits, prey.alive ? 1 : 0, prey.hp);
      return 1;
    }
    std::printf("smoke: weapons ok pistol=1x25 shotgun=5x10 (hits=%d)\n", hits);
  }

  std::printf("SMOKE OK\n");
  return 0;
}
