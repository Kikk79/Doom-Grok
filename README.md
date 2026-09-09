# Doom-Grok

C++17 / SDL2 raycasting Doom-like skeleton. Fixed timestep (1/60), 1 tile = 1.0 world unit.

## Dependencies

- CMake ≥ 3.16
- C++17 compiler (GCC/Clang/MSVC)
- SDL2 development libraries

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

Run from the repo root so `levels/demo.map` resolves. You can also set `DOOM_GROK_ROOT` to the project directory.

Headless logic smoke (no display; doors + pickups + monsters/AI):
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

## Layout

```
app/                 — main.cpp, SDL bootstrap, game loop, HUD, pickups
engine/
  renderer/          — raycast (internal; not public game/ai API)
  camera/            — Camera + Vec2
  collision/         — move, hits_wall, raycast
  timing/            — fixed timestep 1/60
  input/             — keyboard + mouse delta
  map/               — tile map loader + set_tile / try_open_door
game/                — player pose, movement, hitscan, use/doors, vitals
ai/                  — Monster spawn, Idle/Chase AI, hitscan, billboards
levels/demo.map      — 16×16 demo level
```

## Map format (`levels/demo.map`)

```
# doom-map v1
width 16
height 16
# grid rows: . Empty  # Wall  D DoorClosed  O DoorOpen
# entities: P/M/I/E x y type_id  (world coords at tile centers)
```

## License

MIT — skeleton for experimentation.
