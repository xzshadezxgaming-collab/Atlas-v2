# Atlas

A 2D game engine inspired by the technology behind **Cortex Command**:
fully destructible per-pixel terrain, pixel-level physics, and
material-driven gameplay — rebuilt on modern C++ and SDL3.

## Current features (v0.2.0)

- **Per-pixel destructible terrain** — the world is a material grid
  (grass, dirt, stone, gold veins) paired with a streamed GPU texture.
  Every collision query, dig, and edit works on individual pixels.
- **Procedural world generation** — rolling, seeded landscapes with
  layered materials and scattered gold veins.
- **Pixel-perfect character physics** — sub-pixel movement integration
  (no tunneling), automatic step-up over small ledges, and downhill
  ground snapping so bodies walk naturally over craters and slopes.
- **Terrain editing** — dig with the left mouse button, place dirt with
  the right.
- **Fixed-timestep simulation** decoupled from rendering.

### Controls

| Input | Action |
|---|---|
| A / D | Move |
| Space | Jump |
| Left mouse | Dig |
| Right mouse | Place dirt |

## Building

### Windows (Visual Studio)

Uses the CMake presets in `CMakePresets.json` (Ninja + MSVC). The SDL3
headers are vendored in `ThirdParty/sdl3`; the binaries are not in the
repository — drop `SDL3.lib` and `SDL3.dll` into
`ThirdParty/sdl3/lib/x64/` (see `ThirdParty/sdl3/INSTALL.md`), then open
the folder in Visual Studio or run:

```
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

### Linux / macOS

Install SDL3 (from your package manager, or build it from
https://github.com/libsdl-org/SDL), then:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/Game/Atlas
```

## Roadmap

- [x] **Milestone 1 — pixel terrain**: material grid, digging, per-pixel
  character collision
- [ ] **Milestone 2 — pixel particles**: bullets, debris, and gibs as
  single-pixel physics objects with DDA terrain collision; knocked-loose
  terrain pixels that fall and settle back into the world
- [ ] **Milestone 3 — actors**: silhouette-sampled collision, weapons,
  gibbing, a simple AI target
- [ ] **Milestone 4 — game layer**: scenes, actors, and weapons defined
  in data files
