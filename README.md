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

Headless logic smoke (no display; doors + pickups + monsters/AI/hitscan):
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
| LMB / Space / Ctrl | Fire hitscan (damages nearest monster along ray, else wall) |
| E / F | Use — raycast ~1 unit ahead → `try_open_door` (`DoorClosed`→`DoorOpen`) |
| Esc | Quit |

Pose is owned by `game/Player`; the camera follows via `Camera::set_pose` each frame. Monster AI also uses **Player pose**, not Camera.

### Slice 3 gameplay

- **Doors:** `Map::set_tile(tx, ty, Tile)` (false if OOB) and `Map::try_open_door(tx, ty)` (`DoorClosed`→`DoorOpen` only). Use (E/F) raycasts ~1 unit ahead then `try_open_door`. Does not rewrite the map file. `DoorOpen` is not solid.
- **Pickups:** Item spawns (`I x y type_id`) become a runtime list. Distance < ~0.5 collects and removes the entry. `type_id 10` → +25 health; `type_id 11` → +10 armor (+ ammo stub). Player health starts at **100**.
- **HUD:** simple health (green/red) + armor bars drawn on the raycast framebuffer.

### Slice 4 — first monsters / AI

- **Spawn:** `M x y type_id` entities from `Map::spawns()` → `AI::Monster` (`AI::spawn_from_map`). HP by `type_id` (1→40, 2→60, else 30).
- **AI:** `Idle` → `Chase` when clear LOS to the player (`Collision::hits_wall` / raycast). Chase moves with `Collision::move` toward **Player** `pos`.
- **Hitscan:** `Player::try_fire` damages the nearest living monster along the aim ray if closer than the wall hit; death sets `alive=false` (deactivated, no billboard).
- **Visual:** living monsters drawn as colored billboards / filled columns (internal renderer; depth-tested against walls).
- **Collision:** soft radius separation vs the player (monsters are not hard walls).

## Layout

```
app/                 — main.cpp, SDL bootstrap, game loop, HUD, pickups, smoke
engine/
  renderer/          — raycast + billboards (internal; not public game/ai API)
  camera/            — Camera + Vec2
  collision/         — move, hits_wall, raycast
  timing/            — fixed timestep 1/60
  input/             — keyboard + mouse delta
  map/               — tile map loader + set_tile / try_open_door
game/                — player pose, movement, hitscan, use/doors, vitals
ai/                  — Monster spawn, Idle/Chase AI, hitscan damage, soft collide
levels/demo.map      — 16×16 demo level (3 monsters, 2 items)
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
