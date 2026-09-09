int run_smoke(const char* map_path) {
  Map map;
  if (!map.load(map_path)) {
    std::fprintf(stderr, "SMOKE FAIL: load %s\n", map_path);
    return 1;
  }
  int door_tx = -1, door_ty = -1;
  for (int y = 0; y < map.height(); ++y) {
    for (int x = 0; x < map.width(); ++x) {
      if (map.tile_at(x, y) == Tile::DoorClosed) {
        door_tx = x; door_ty = y; break;
      }
    }
    if (door_tx >= 0) break;
  }
  if (door_tx < 0) {
    std::fprintf(stderr, "SMOKE FAIL: no DoorClosed in map\n");
    return 1;
  }
  if (!map.try_open_door(door_tx, door_ty)) {
    std::fprintf(stderr, "SMOKE FAIL: try_open_door\n");
    return 1;
  }
  if (map.tile_at(door_tx, door_ty) != Tile::DoorOpen || map.is_solid(door_tx, door_ty)) {
    std::fprintf(stderr, "SMOKE FAIL: DoorOpen still solid or wrong tile\n");
    return 1;
  }
  std::printf("smoke: door (%d,%d) opened, walkable=%d\n",
              door_tx, door_ty, map.is_solid(door_tx, door_ty) ? 0 : 1);

  Player player;
  player.health = 100;
  player.armor = 0;
  std::vector<ActivePickup> pickups;
  for (const auto& s : map.spawns()) {
    if (s.kind == EntitySpawn::Kind::Item) pickups.push_back({s.pos, s.type_id});
  }
  if (pickups.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: no item spawns\n");
    return 1;
  }
  const size_t before = pickups.size();
  player.pos = pickups[0].pos;
  collect_pickups(player, pickups);
  if (pickups.size() != before - 1) {
    std::fprintf(stderr, "SMOKE FAIL: pickup not collected\n");
    return 1;
  }
  if (player.health < 100 && player.armor <= 0 && player.ammo <= 0) {
    std::fprintf(stderr, "SMOKE FAIL: no pickup effect applied\n");
    return 1;
  }
  std::printf("smoke: pickups %zu -> %zu health=%d armor=%d ammo=%d\n",
              before, pickups.size(), player.health, player.armor, player.ammo);
  if (map.set_tile(-1, 0, Tile::Empty) || map.set_tile(map.width(), 0, Tile::Empty)) {
    std::fprintf(stderr, "SMOKE FAIL: OOB set_tile should fail\n");
    return 1;
  }
  if (map.try_open_door(-1, 0) || map.try_open_door(0, 0)) {
    std::fprintf(stderr, "SMOKE FAIL: try_open_door should reject non-doors\n");
    return 1;
  }

  // --- Slice 4 (light): spawn + Chase on LOS + hitscan kill ---
  std::vector<AI::Monster> monsters = AI::spawn_from_map(map);
  if (monsters.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: expected monster spawns\n");
    return 1;
  }
  std::printf("smoke: spawned %zu monsters\n", monsters.size());
  for (const auto& m : monsters) {
    if (!m.alive || m.hp != m.max_hp || m.max_hp <= 0) {
      std::fprintf(stderr, "SMOKE FAIL: monster hp init type_id=%d\n", m.type_id);
      return 1;
    }
  }

  AI::Monster& prey = monsters[0];
  player.pos = {prey.pos.x - 2.0f, prey.pos.y};
  player.dir = {1.0f, 0.0f};
  if (Collision::hits_wall(map, prey.pos, player.pos)) {
    player.pos = {7.5f, 8.5f};
    prey.pos = {9.5f, 8.5f};
  }
  const float dist0 = std::hypot(prey.pos.x - player.pos.x, prey.pos.y - player.pos.y);
  for (int i = 0; i < 60; ++i) {
    AI::update(monsters, map, player, 1.0f / 60.0f);
  }
  // May already be Attack if chase closed inside melee range — both mean aggro.
  if (prey.state != AI::State::Chase && prey.state != AI::State::Attack) {
    std::fprintf(stderr, "SMOKE FAIL: monster did not enter Chase/Attack on LOS (state=%d)\n",
                 static_cast<int>(prey.state));
    return 1;
  }
  const float dist1 = std::hypot(prey.pos.x - player.pos.x, prey.pos.y - player.pos.y);
  if (!(dist1 < dist0 - 0.05f) && prey.state != AI::State::Attack) {
    std::fprintf(stderr, "SMOKE FAIL: chase did not close (%.3f -> %.3f)\n", dist0, dist1);
    return 1;
  }
  std::printf("smoke: chase ok dist %.3f -> %.3f\n", dist0, dist1);

  player.pos = {prey.pos.x - 1.5f, prey.pos.y};
  player.dir = {1.0f, 0.0f};
  prey.alive = true;
  prey.hp = prey.max_hp;
  int shots = 0;
  while (prey.alive && shots < 16) {
    if (!AI::apply_hitscan(map, monsters, player.pos, player.dir, 25)) {
      std::fprintf(stderr, "SMOKE FAIL: hitscan missed living monster\n");
      return 1;
    }
    ++shots;
  }
  if (prey.alive) {
    std::fprintf(stderr, "SMOKE FAIL: monster not killed after %d shots\n", shots);
    return 1;
  }
  std::printf("smoke: hitscan kill in %d shots\n", shots);

  // --- Slice 5: melee Attack calls Player::take_damage ---
  monsters = AI::spawn_from_map(map);
  AI::Monster& bruiser = monsters[0];
  bruiser.alive = true;
  bruiser.hp = bruiser.max_hp;
  bruiser.state = AI::State::Chase;
  bruiser.attack_cd = 0.0f;
  player.reset_vitals(player.pos, player.dir);
  player.armor = 0;
  player.health = 100;
  player.invuln_t = 0.0f;
  // Place player inside melee range with clear LOS.
  player.pos = {bruiser.pos.x + 0.4f, bruiser.pos.y};
  if (Collision::hits_wall(map, bruiser.pos, player.pos)) {
    bruiser.pos = {8.5f, 8.5f};
    player.pos = {9.0f, 8.5f};
  }
  const int expect_dmg = AI::melee_damage_for(bruiser.type_id);
  const int hp_before = player.health;
  AI::update(monsters, map, player, 1.0f / 60.0f);
  if (bruiser.state != AI::State::Attack) {
    std::fprintf(stderr, "SMOKE FAIL: expected Attack state got %d\n",
                 static_cast<int>(bruiser.state));
    return 1;
  }
  if (player.health != hp_before - expect_dmg) {
    std::fprintf(stderr, "SMOKE FAIL: melee dmg expected hp=%d got %d (type_id=%d dmg=%d)\n",
                 hp_before - expect_dmg, player.health, bruiser.type_id, expect_dmg);
    return 1;
  }
  if (bruiser.attack_cd <= 0.0f) {
    std::fprintf(stderr, "SMOKE FAIL: attack cooldown not set\n");
    return 1;
  }
  // Still on cooldown: no second swing even if i-frames cleared.
  player.invuln_t = 0.0f;
  const int hp_mid = player.health;
  AI::update(monsters, map, player, 0.1f);
  if (player.health != hp_mid) {
    std::fprintf(stderr, "SMOKE FAIL: melee fired during cooldown\n");
    return 1;
  }
  // Advance past cooldown → another hit.
  AI::update(monsters, map, player, AI::kMeleeCooldown);
  if (player.health != hp_mid - expect_dmg) {
    std::fprintf(stderr, "SMOKE FAIL: melee after cooldown expected hp=%d got %d\n",
                 hp_mid - expect_dmg, player.health);
    return 1;
  }
  std::printf("smoke: melee Attack ok dmg=%d cd=%.2f range=%.2f\n",
              expect_dmg, AI::kMeleeCooldown, AI::kMeleeRange);

  // i-frames: second direct take_damage blocked, then after >0.75s lands again.
  player.invuln_t = 0.0f;
  if (!player.take_damage(10) || !player.invulnerable()) {
    std::fprintf(stderr, "SMOKE FAIL: take_damage / i-frames arm\n");
    return 1;
  }
  if (player.take_damage(10)) {
    std::fprintf(stderr, "SMOKE FAIL: i-frames should block\n");
    return 1;
  }
  player.tick_fx(Player::kIFrameSec + 0.05f);
  if (player.invulnerable() || !player.take_damage(10)) {
    std::fprintf(stderr, "SMOKE FAIL: post-iframe damage\n");
    return 1;
  }
  std::printf("smoke: i-frames ok\n");

  // Death + restart
  player.invuln_t = 0.0f;
  player.armor = 0;
  player.health = 5;
  if (!player.take_damage(10) || player.alive()) {
    std::fprintf(stderr, "SMOKE FAIL: expected death\n");
    return 1;
  }
  Camera cam;
  if (!reload_level(map_path, map, player, cam, monsters, pickups)) {
    std::fprintf(stderr, "SMOKE FAIL: reload_level\n");
    return 1;
  }
  if (player.health != 100 || !player.alive() || monsters.empty() || pickups.empty()) {
    std::fprintf(stderr, "SMOKE FAIL: restart state\n");
    return 1;
  }
  std::printf("smoke: death+restart ok\n");

