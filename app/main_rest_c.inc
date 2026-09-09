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

  std::printf("SMOKE OK\n");
  return 0;
}
