# Doom-Grok

C++17 / SDL2 raycasting Doom-like skeleton. Fixed timestep (1/60), 1 tile = 1.0 world unit.

## Dependencies

- CMake ≥ 3.16
- C++17 compiler (GCC/Clang/MSVC)
- SDL2 development libraries (video + **audio**; no SDL2_mixer / wav assets required)

### Install SDL2

**Debian/Ubuntu**
```bash
sudo apt-get install build-essential cmake pkg-config libsdl2-dev
```

**Fedora**
```bash
sudo dnf install cmake gcc-c++ SDL2-devel
```

**macOS (Homebrew)**
```bash
brew install cmake sdl2
```

**Windows (vcpkg)**
```bash
vcpkg install sdl2
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
./build/doom-grok
```

Run from the repo root so `levels/demo.map` / `levels/e1m2.map` resolve. You can also set `DOOM_GROK_ROOT` to the project directory.

Headless logic smoke (no display / **no audio hardware**; doors + pickups + monsters/AI + melee/damage/restart + win + two-map campaign load; textures are renderer-only; SFX skipped):
```bash
./build/doom-grok --smoke
```

Dummy-video smoke (window path):
```bash
SDL_VIDEODRIVER=dummy ./build/doom-grok &
sleep 1; kill %1 2>/dev/null || true
```

## Controls

| Key | Action |
|-----|--------|
| W / ↑ | Move forward |
| S / ↓ | Move backward |
| A | Strafe left |
| D | Strafe right |
| ← / → | Turn |
| Mouse | Look (yaw) |
| LMB / Space / Ctrl | Fire hitscan (raycast; wall hit logs dist + brief flash) |
| E / F | Use — raycast ~1 unit ahead → `try_open_door` (`DoorClosed`→`DoorOpen`) |
| R | Restart current level (after game over / YOU WIN / campaign complete) |
| N | Next map after YOU WIN (demo → e1m2); no-op on e1m2 COMPLETE |
| Esc | Quit |

Pose is owned by `game/Player`; the camera follows via `Camera::set_pose` each frame.

### Slice 3 gameplay

- **Doors:** `Map::set_tile(tx, ty, Tile)` (false if OOB) and `Map::try_open_door(tx, ty)` (`DoorClosed`→`DoorOpen` only). Use (E/F) raycasts ~1 unit ahead then `try_open_door`. Does not rewrite the map file. `DoorOpen` is not solid.
- **Pickups:** Item spawns (`I x y type_id`) become a runtime list. Distance < ~0.5 collects and removes the entry. `type_id 10` → +25 health; `type_id 11` → +10 armor (+ ammo stub). Player health starts at **100**.
- **HUD:** simple health (green/red) + armor bars drawn on the raycast framebuffer.


## Slice 4 — monsters / simple AI

- **Monsters:** `Map` spawns `Kind::Monster` + `type_id` become runtime `AI::Monster` via `AI::spawn_from_map`.
- **AI states:** `Idle` (default) → `Chase` when clear LOS to player (`Collision::hits_wall`). Chase moves toward `player.pos` with `Collision::move` (radius ~0.25).
- **Types:** `type_id 1` Imp-like — HP 30, speed 2.0, reddish. `type_id 2` (and default) — HP 50, speed 1.5, greenish/blue.
- **Hitscan:** On successful `Player::try_fire`, app calls `AI::apply_hitscan(map, monsters, pos, dir, 25)` — closest alive monster on the ray before a wall takes damage; dies at hp ≤ 0. Wall logging stays inside `try_fire`.
- **Draw:** `AI::draw(fb, camera, map, monsters)` — classic camera-space colored vertical strips; skips if a wall occludes cam→monster. No new Engine public APIs.
- **Smoke:** `--smoke` still covers doors/pickups; lightly extends to spawn count, Chase-on-LOS, and hitscan kill.

Layout note: `ai/` is no longer stubs — `ai/ai.hpp` + `ai/ai.cpp`.

## Slice 5 — monster melee + player damage

- **Attack:** dist ≤ `AI::kMeleeRange` (0.55) and (LOS or Chase/Attack) → `State::Attack`.
- **Melee:** AI-owned cooldown `AI::kMeleeCooldown` (0.9s). Calls `player.take_damage(type_id==2 ? 15 : 10)`.
- **Player:** armor absorbs ~half each hit; i-frames `Player::kIFrameSec` (0.75s) via `invuln_t`.
- **API:** `take_damage`, `alive()`, `invulnerable()`, `reset_vitals(spawn_pos, spawn_dir)`.
- **Death / restart:** HP ≤ 0 → game over overlay; **R** reloads map + resets player/monsters/pickups.
- **Smoke:** melee Attack, i-frames, death, `reload_level`.
- Engine APIs unchanged — `ai/` + `game/` only.

## Slice 6 — win condition

- **Clear:** when every spawned monster has `!alive`, the level is won.
- **YOU WIN:** green/gold overlay (distinct from the red GAME OVER banner).
- **Exit (optional):** map entity `E` → `EntitySpawn::Kind::Door` — walking into it after clear also wins; all-dead alone is enough.
- **Restart:** **R** from win or death reloads the map, respawns monsters/pickups, and `reset_vitals`.
- **Smoke:** kill-all → win flag; reload clears win.
- Engine / Player / AI public APIs unchanged — app-loop only.

## Slice 7 — wall textures

- **Textures:** tiny procedural 64×64 atlas (brick, tech panel, door) — no external image assets.
- **Sampling:** raycaster uses wall hit `side` + fractional wall X (`map` cell / hit point) and draws textured vertical strips instead of flat colors.
- **Classic look:** EW (`side==1`) faces are darkened vs NS faces.
- **Doors:** `DoorClosed` uses a distinct wood/brass door texture (brighter tint). `DoorOpen` stays non-solid (see-through opening) so it remains visually distinct.
- **API:** `Collision::RayHit` gains additive `side` (0=NS/x-step, 1=EW/y-step). `Renderer::raycast_view` signature unchanged; public game/ai APIs unchanged.
- **Smoke:** `--smoke` still passes (logic path; no framebuffer asserts).

## Slice 8 — second level + progression

Level list: `{"levels/demo.map", "levels/e1m2.map"}` (app wiring; Map::load already path-based).

- **e1m2:** `levels/e1m2.map` — 16×16, two chambers linked by `DD` doors; 5 monsters (3×type1, 2×type2); 3 items (2×health, 1×armor); `P` NW start; `E` SE exit after clear. Harder than demo.
- **R:** restart current level (reload same path; reset vitals/pose from `P`, respawn monsters/items).
- **N (after win on demo):** load `e1m2.map` fresh.
- **After win on e1m2:** campaign COMPLETE overlay — **no wrap**; **N** is a no-op; **R** restarts `e1m2`.
- **Win overlay hint:** N next · R restart (on final map: R restart only).
- **Tracking:** app keeps `current_level` path; `reload_level` uses it.
- **Smoke:** loads both maps + progression (demo→e1m2, no wrap after e1m2).
- **APIs:** Engine / Player / AI public APIs unchanged — app-loop + map assets only.

## Slice 9 — simple SFX

- **Audio:** `engine/audio/` — SDL2 audio device (`SDL_QueueAudio`) with **procedural** short beeps/noises (no wav / mixer assets). Optional SDL2_mixer not used.
- **Triggers:** fire/hitscan, door open (`try_use`), player hurt (HP drop), pickup collect, monster death (hitscan kill), win jingle (first win transition).
- **Mute-safe:** if `SDL_INIT_AUDIO` / `OpenAudioDevice` fails, the game still runs; `Audio::play` is a no-op.
- **Smoke:** `--smoke` does **not** init audio (no device required).
- **APIs:** public game / AI / other engine APIs unchanged — small `Audio` module only.

## Layout

```
app/                 — main.cpp, SDL bootstrap, game loop, HUD, pickups, win / campaign progression
engine/
  audio/             — procedural SFX (SDL2 device; mute-safe)
  renderer/          — textured raycast (internal; not public game/ai API)
  camera/            — Camera + Vec2
  collision/         — move, hits_wall, raycast
  timing/            — fixed timestep 1/60
  input/             — keyboard + mouse delta
  map/               — tile map loader + set_tile / try_open_door
game/                — player pose, movement, hitscan, use/doors, vitals, damage
ai/                  — Monster spawn, Idle/Chase/Attack AI, hitscan, melee, billboards
levels/demo.map      — 16×16 campaign map 1
levels/e1m2.map      — 16×16 campaign map 2 (two chambers)
```

## Map format (`levels/*.map`)

```
# doom-map v1
width 16
height 16
# grid rows: . Empty  # Wall  D DoorClosed  O DoorOpen
# entities: P/M/I/E x y type_id  (world coords at tile centers)
```

## License

MIT — skeleton for experimentation.
