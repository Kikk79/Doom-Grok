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

Headless logic smoke (no display; doors + pickups):
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
| E / F | Use — open facing/adjacent `DoorClosed` within ~1 unit → `DoorOpen` (walkable) |
| Esc | Quit |

Pose is owned by `game/Player`; the camera follows via `Camera::set_pose` each frame.

### Slice 3 gameplay

- **Doors:** `Map::try_set_tile(tx, ty, Tile)` is the minimal runtime mutator (does not rewrite the map file). `DoorOpen` is not solid.
- **Pickups:** Item spawns (`I x y type_id`) become a runtime list. Distance < ~0.5 collects and removes the entry. `type_id 10` → +25 health; `type_id 11` → +10 armor (+ ammo stub). Player health starts at **100**.
- **HUD:** simple health (green/red) + armor bars drawn on the raycast framebuffer.

## Layout

```
app/                 — main.cpp, SDL bootstrap, game loop, HUD, pickups
engine/
  renderer/          — raycast (internal; not public game/ai API)
  camera/            — Camera + Vec2
  collision/         — move, hits_wall, raycast
  timing/            — fixed timestep 1/60
  input/             — keyboard + mouse delta
  map/               — tile map loader + try_set_tile mutator
game/                — player pose, movement, hitscan, use/doors, vitals
ai/                  — AI stubs
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
