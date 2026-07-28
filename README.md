# Atlas

A 2D game engine inspired by the technology behind **Cortex Command**:
fully destructible per-pixel terrain, pixel-level physics, and
material-driven gameplay — rebuilt on modern C++ and SDL3.

## Current features (v0.5.0)

- **Per-pixel destructible terrain** — the world is a material grid
  (grass, dirt, stone, gold veins) paired with a streamed GPU texture.
  Every collision query, dig, and edit works on individual pixels.
- **Procedural world generation** — rolling, seeded landscapes with
  layered materials and scattered gold veins.
- **Limb-based actor physics** — Cortex Command-style walkers: the torso
  is one hitbox and each leg is a limb with its own foot hitbox. Feet
  find and hold real footholds in the pixel terrain, plant at different
  heights on slopes, step over rubble, and lose grip when the ground
  under them is dug away. Legs render with IK-bent knees.
- **Crouch** — holding S folds the legs and lowers the torso so the
  actor can wriggle through tight dug tunnels and low passages.
- **Pixel particle combat** — bullets are single-pixel physics objects
  that trace through the terrain (no tunneling) and test their energy
  against material strength: dirt penetrates, stone stops. Knocked-loose
  pixels fall as debris and settle back into the world as new terrain;
  blood stains it.
- **Weapons on an IK aim arm** — SMG, shotgun, rifle, a terrain
  digger, and a shovel, aimed with the mouse, with recoil, spread,
  clips and reloads; switch with 1-5 or the scroll wheel.
  All stats data-driven from `Assets/weapons.ini` — tune or add weapons
  without recompiling.
- **Specialized dig tools** — the digger is a rock/ore specialist:
  cheap and fast against stone and gold, sluggish in dirt. The shovel
  is an earthmover: huge cheap scoops of dirt/grass, but its blade
  physically can't bite into stone or gold at all. Both erode
  material gradually (a strength budget per swing) rather than
  deleting a circle of terrain outright.
- **Gold currency** — destroying gold pixels (by digger, bullet, or
  explosion) pays out currency to whoever did it, tracked per actor
  and shown in the HUD, ready for a future shop/delivery system.
- **Grenades and explosions** — bouncing grenades with fuses; blasts
  carve craters, fling debris, and knock actors back.
- **Health, gore, and gibbing** — bullet/explosion/fall damage,
  blood spray that stains terrain, and actors bursting into chunks.
- **Jetpack** — hold-to-thrust with fuel that regenerates on the ground.
- **Enemy AI and waves** — red enemy soldiers patrol, spot you through
  real terrain line-of-sight, and fire in bursts; endless waves with
  respawns. HUD bars for health, fuel and ammo.
- **Procedural retro SFX** — gunshots, explosions, digging, jumps and
  gibs synthesized at startup (SDL3 audio), no sound files needed.
- **Data-driven scenes** — world size and seed come from
  `Assets/scene.ini` (seed 0 = new world every launch).
- **Atmosphere** — dusk sky with stars, moon and drifting clouds over
  three parallax mountain layers; screen-edge vignette; screen shake;
  medkit drops from fallen enemies.
- **Lighting and rendering** — sunlit terrain edges and dark overhangs,
  depth-darkened underground, per-material texture (mottled stone,
  dirt specks, glittering gold), glowing tracer rounds, additive
  fireballs and muzzle light, ejected shell casings, scorched crater
  rims, walk bob and lean, and hit flashes on actors.
- **Fixed-timestep simulation** decoupled from rendering.

### Controls

| Input | Action |
|---|---|
| A / D | Move |
| Space | Jump |
| S | Crouch (fits through tight passages) |
| W / Left Shift | Jetpack |
| Mouse | Aim |
| Left mouse | Fire |
| Right mouse | Throw grenade |
| 1-5 / scroll wheel | SMG / Shotgun / Rifle / Digger / Shovel |
| R | Reload |

## Building

### Windows (Visual Studio)

Uses the CMake presets in `CMakePresets.json` (Ninja + MSVC). SDL3
headers and binaries (3.4.10) are vendored in `ThirdParty/sdl3` — no
setup needed. Open the folder in Visual Studio or run:

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
- [x] **Milestone 2 — pixel particles**: bullets, debris and gibs as
  single-pixel physics objects with DDA terrain collision; knocked-loose
  terrain that settles back into the world
- [x] **Milestone 3a — limb walkers**: per-foot terrain collision,
  walking gait with real footholds, IK leg rendering
- [x] **Milestone 3b — combat actors**: aim arm, weapons, grenades,
  gibbing, enemy AI, waves
- [ ] **Milestone 4 — game layer**: scenes and props from data files,
  inventory/pickups, a gold-spending shop/drop-ship delivery system
  (currency already mineable and tracked), objectives
- [ ] **Milestone 5 — feel and polish**: body pitch and stagger,
  screen shake, parallax background, performance pass, settings
