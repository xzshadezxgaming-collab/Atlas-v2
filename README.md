# Atlas

A 2D game engine inspired by the technology behind **Cortex Command**:
fully destructible per-pixel terrain, pixel-level physics, and
material-driven gameplay — rebuilt on modern C++ and SDL3.

## Download and play (Windows)

**[Download Atlas-Windows.zip](https://github.com/xzshadezxgaming-collab/atlas-v2/releases/latest/download/Atlas-Windows.zip)**,
unzip it anywhere and double-click `Atlas.exe`. If Windows SmartScreen
warns about it, click **More info → Run anyway** (the build isn't
code-signed). For co-op, one player runs `Host co-op.bat` and the others
run `Join co-op.bat` with the host's IP; `README.txt` in the zip covers
LAN and internet play.

Every push to the default branch builds a new zip and publishes it as
a [release](https://github.com/xzshadezxgaming-collab/atlas-v2/releases)
(`.github/workflows/release.yml`), so that link is always the newest
build.

## Current features (v0.11.0)

- **Modern presentation** — the battlefield is drawn closer (1.5x by
  default; 1x to 2x under Settings, WORLD ZOOM) with sharp pixel-art
  scaling, and the camera glides at sub-pixel precision instead of
  stepping a pixel at a time. Bright light (muzzle flashes, fire, lava,
  explosions, the moon) blooms into a soft glow, and nearby blasts wash
  the screen with warm light (BLOOM in Settings). All text uses an
  anti-aliased font (Chakra Petch) rendered at your screen's real
  resolution, so it stays crisp in fullscreen. Menus, buttons and HUD
  panels share a new chamfered glass style with eased hover effects.

- **New look** — dynamic lighting: caves and tunnels are genuinely
  dark, muzzle flashes, explosions, fire and lava light up the rock
  around them, and your body carries a soft lamp. Terrain has bevelled,
  moonlit top edges, shaded undersides and contact shadows inside
  tunnels. The sky is a smooth dusk gradient with a galaxy band,
  twinkling stars, a shaded moon and soft drifting clouds over five
  layers of parallax ridges with haze between them. Smoke and dust are
  soft puffs, tracers are tapered streaks, blasts send out a smooth
  shock ring. A cleaner HUD puts vitals and your weapon in panels at the
  bottom left with segmented bars and big numbers, small text uses a
  crisp new 5x7 font, menus have a refreshed style, and fullscreen now
  scales the game instead of leaving it in a corner. Lighting can be
  turned off in Settings.

- **Steady camera** — screen shake fades out again after a hit (it
  had been stuck at full strength), is gentler, and can be set to OFF,
  SUBTLE or STRONG in Settings; the aim lookahead no longer wobbles.

- **Art and sound packs** — every build now has its own hand-pixelled
  body: the **scout** in a soft cap, goggles and khaki with a radio
  whip, the **rifleman** in olive, and the **heavy** in a full steel
  suit with a glowing visor and armoured jetpack, each with matching
  sleeves. Every weapon has real pixel art held at the grip and turned
  with your aim (SMG, shotgun, rifle, scoped sniper, rocket tube,
  flamethrower, digger, shovel), and the HUD's item slots show each
  weapon's icon. Drop ships, drones, weapon cases, supply crates, drone
  pods, medkits, rockets and grenades are sprites too, and the HUD bars
  have proper icons. A new sound pack replaces the old synth blips with
  layered effects (muzzle blast, body, room and echo tails, pump and
  bolt mechanics, debris crackle, grunts) recorded in several takes so
  rapid fire never repeats one sample, plus new sounds for menu clicks,
  squad orders over the radio, weapon and body switching, medkits,
  pickups, steam, the brain alarm, sending an order, drop ships flying
  over, and victory and defeat stingers. Both packs are generated from
  source by scripts in `Tools/` (see **Assets** below), and the game
  falls back to its built-in drawing and synth when a pack is missing.

- **Smarter enemy, your landing zone** — the enemy commander now runs
  on a real budget: it earns gold over time (faster on harder
  difficulties) and from every kill, and spends it on the bodies,
  weapons and drones it ships in, so a winning enemy gets stronger
  and a starved one sends less. Its attackers carry diggers and, when
  a wall or hill stops them, they tunnel through it instead of
  standing still. On the skirmish and host screens you get a preview
  of the actual map (random seeds are rolled up front so you see what
  you'll play) and you click it to choose where your bunker lands:
  further west is safer, further east is closer to the fight. An
  in-game **minimap** (M) shows the terrain, both brains, every unit
  and your view.

- **Sound and music** — a procedural soundtrack with a calm loop and a
  combat loop over the same chords, crossfaded by how much fighting is
  going on near you, with its own volume slider. Every sound in the
  world is now positional: it fades with distance from your view and
  pans left or right, and enemy, ally and drone gunfire is voiced too.


- **Arsenal and bodies** — three builds to order: the fast, fragile
  **SCOUT** (70 health, quicker legs and jetpack), the **RIFLEMAN**
  (100), and the armored **HEAVY** (200 health, takes 30% less from
  every hit, slow and thirsty on jet fuel), each drawn with its own
  gear. Every body carries its own inventory of up to six items, and
  whoever drives a body uses what it carries: switch bodies with Q and
  you get that body's kit, pick up a weapon crate and it joins your
  items instead of replacing them. New weapons: a **sniper rifle**
  with a laser sight that punches deep into rock, a **rocket
  launcher** whose rockets accelerate and burst on contact, and a
  **flamethrower** that sets bodies alight (water puts them out) and
  burns grass away; lava sets you on fire too. **Drones** are small
  flying gun platforms you can order: they hover over the ground,
  follow your squad orders and shoot what they can see, and the enemy
  starts sending its own from the third drop on, along with heavies
  carrying rockets and scouts with shotguns and flamethrowers.

- **Menus and screens** — a main menu over a live AI battle, skirmish
  and host setup (mode, map size, difficulty, seed, port), a join
  screen, settings (volume, screen shake, fullscreen, FPS counter,
  saved between runs), a controls page, an Esc pause menu (restart,
  settings, back to menu), and a victory/defeat screen with match
  stats. `--host`, `--join` and `--survival` still jump straight in.

- **Brain assault** — the default match, Cortex Command's core loop.
  Each side has a brain in a glass jar inside a concrete bunker at its
  end of the map. Destroy theirs, keep yours alive. Enemy drop ships
  land reinforcements near their base on a timer and march them on
  your bunker, jetting over whatever is in the way, while guards hold
  theirs. Your side's bodies are interchangeable: press **Q** to jump
  into the next one (the body you leave carries on under AI), and when
  your body dies you take over the nearest survivor. Squad orders
  (**Z** defend, **X** follow me, **C** attack) direct your AI units.
  Gold is pooled per team; kills pay a bounty. The brain is made of
  terrain pixels, so bullets and blasts can kill it, but never your
  own team's. Concrete stops bullets and shrugs off most of a blast.
  `--survival` plays the old endless waves instead.

- **Per-pixel destructible terrain** — the world is a material grid
  (grass, dirt, stone, gold veins) paired with a streamed GPU texture.
  Every collision query, dig, and edit works on individual pixels.
- **Living terrain** — a falling-sand simulation runs on the material
  grid. Sand and gravel slide and pile when undermined; water and lava
  flow, pool and level out; lava meeting water flashes to steam and
  hardens to obsidian. Only 32x32 regions where something can still
  move are simulated, so a settled world costs almost nothing. Lava
  burns, water slows you down, and an indestructible bedrock floor
  keeps anyone from digging out of the map.
- **Procedural world generation** — seeded landscapes with a dirt
  crust over stone, desert dunes, sand and gravel pockets, gold veins
  that get richer with depth, winding cave systems with chambers, and
  level pools of water (lava further down). Dug-out space shows the
  rock wall behind it.
- **Limb-based actor physics** — Cortex Command-style walkers: the torso
  is one hitbox and each leg is a limb with its own foot hitbox. Feet
  find and hold real footholds in the pixel terrain, plant at different
  heights on slopes, step over rubble, and lose grip when the ground
  under them is dug away. Legs render with IK-bent knees; the rendered
  foot position eases toward instantaneous logical repositions (an
  emergency recovery plant, a landing catch) so the gait always reads
  as a smooth step rather than a pop, without touching the underlying
  collision/support logic at all.
- **Crouch** — holding S folds the legs and lowers the torso so the
  actor can wriggle through tight dug tunnels and low passages.
- **Pixel particle combat** — bullets are single-pixel physics objects
  that trace through the terrain (no tunneling) and test their energy
  against material strength: dirt penetrates, stone stops. Knocked-loose
  pixels fall as debris and settle back into the world as new terrain;
  blood stains it.
- **Weapons on an IK aim arm** — SMG, shotgun, rifle, a terrain
  digger, and a shovel, aimed with the mouse, with recoil, spread,
  clips and reloads; switch with 1-6 or the scroll wheel.
  All stats data-driven from `Assets/weapons.ini` — tune or add weapons
  without recompiling.
- **Specialized dig tools** — the digger is a rock/ore specialist:
  cheap and fast against stone and gold, sluggish in dirt. The shovel
  is an earthmover: huge cheap scoops of dirt/grass, but its blade
  physically can't bite into stone or gold at all. Both erode
  material gradually (a strength budget per swing) rather than
  deleting a circle of terrain outright, and dig silently with no
  flying debris - that scatter is reserved for explosions. The
  digger draws a rotating double-helix plasma beam from the tool tip
  to the point of impact while cutting, and rather than staying
  perfectly straight, its effective direction smoothly sweeps back
  and forth in a cone centered on the aim (data-driven per weapon via
  `ConeAngleDegrees`/`ConeSweepSpeed` in `weapons.ini`).
- **Independent, destructible limbs** — head, torso, arm and each leg
  are separate hitboxes, every one with its own small destructible
  pixel grid. Bullets and the digger's beam (via `LimbDamage` in
  `weapons.ini`) chew into whichever limb they land on. Losing the
  head or torso is fatal outright; a destroyed leg can never plant
  again (a hobbled actor keeps fighting on the other one, but losing
  both is fatal); a destroyed arm can no longer aim or fire.
- **Gold currency** — destroying gold pixels (by digger, bullet, or
  explosion) pays out currency to whoever did it, tracked per actor
  and shown in the HUD.
- **Buy menu: a real Cortex Command-style order cart** — hold Tab to
  open a floating wheel above the player; BUY opens an order panel with
  a catalog (SCOUT, RIFLEMAN and HEAVY bodies, every weapon and tool,
  DRONES and SUPPLY CRATES) and a running cart. Buying a body makes it the active
  assignment target: every weapon bought afterward equips onto that
  body (replacing whatever it had), instead of just always handing out
  a generic rifleman. An "EQUIP:" toggle lets you switch which cart
  body a purchase targets, or send it to "LOOSE ITEM" mode instead -
  weapons bought with no active body queue as loose field crates.
  Click a body's line in the cart to make it the target again;
  right-click any cart line to cancel just that item (nothing is spent
  until you hit SEND ORDER). Rows and the send button grey out when
  you can't afford them or (for bodies) when the field is already at
  its ally cap.
  Sending the order spends the total once and queues one Cortex
  Command-style drop-ship delivery per line: reinforcement bodies land
  and fight already equipped with whatever you assigned; loose weapons
  parachute down as a distinct olive weapon case that sits on the
  ground until you walk over it, adding (fully loaded) that weapon to
  your body's items; supply crates heal and refill everything you
  carry; drones unfold and take off. Each delivery ship flies in from off to
  one side and out the other while its payload parachutes down
  independently. The world keeps running while the menu is open, but
  it captures clicks so you don't fire through it.
- **Bots toggle** — a HUD button temporarily turns off new wave
  spawns (existing enemies aren't affected) for testing or a breather.
- **Grenades and explosions** — bouncing grenades with fuses; blasts
  carve craters, fling debris, and knock actors back.
- **Health, gore, and gibbing** — bullet/explosion/fall damage,
  blood spray that stains terrain, and actors bursting into chunks.
- **Jetpack** — hold-to-thrust with fuel that regenerates on the ground.
- **Enemy AI and waves** — red enemy soldiers patrol, spot you through
  real terrain line-of-sight, and fire in bursts; endless waves with
  respawns. HUD bars for health, fuel and ammo.
- **Allies** — reinforcements delivered via the buy menu fight
  alongside you: each one targets whatever enemy is nearest with the
  same AI enemies use. Bullets never hurt a teammate (same-team fire
  passes through), though explosions still don't discriminate.
- **Procedural retro SFX** — gunshots, explosions, digging, jumps and
  gibs synthesized at startup (SDL3 audio), no sound files needed. The
  dig/tool sound is a soft, heavily-lowpassed crumble with a gentle
  attack rather than a harsh static crackle, and rotates between a
  few takes so retriggering rapidly while digging doesn't sound like
  the same click looping.
- **Data-driven scenes** — world size and seed come from
  `Assets/scene.ini` (seed 0 = new world every launch).
- **Atmosphere** — dusk sky with a dense twinkling star field, moon,
  drifting clouds and a warm horizon glow over four parallax
  mountain/ridge layers separated by atmospheric haze bands;
  screen-edge vignette; medkit drops from fallen enemies.
- **Game feel** — smoothed camera that leads toward your aim,
  trauma-based screen shake (explosions, kills, hits and hard
  landings all thump the camera in proportion), and a few frames of
  hit-stop on every kill so impacts land. Floating damage numbers
  rise off enemies, mined gold pops up as a batched "+N G", and each
  wave opens with a center-screen banner.
- **Lighting and rendering** — sunlit terrain edges and dark
  overhangs, depth-darkened underground, per-material chroma grain
  (every pixel leans warm or cool, not just lighter/darker), grass
  tufts breaking the surface line, mottled stone, glittering gold,
  glowing tracer rounds, explosion shockwave rings and light pops,
  additive fireballs, dust puffs on footsteps/landings/bullet
  impacts, grenade smoke trails with accelerating fuse blinks,
  contact shadows under actors, white hit-flashes, idle breathing,
  jetpack lean, arm recoil kick, ejected shell casings, and
  scorched crater rims.
- **HUD** — layered bars with icons and a damage-lag ghost on
  health, drop-shadowed text, a dynamic crosshair that opens with
  weapon spread, blooms on each shot and shows reload progress at
  the cursor, and hover states on clickable buttons.
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
| 1-6 / scroll wheel | Switch between the items your body carries |
| R | Reload |
| Tab (hold) | Open the buy menu |
| Q | Switch to the next friendly body |
| Z / X / C | Squad order: defend / follow / attack |
| M | Toggle the minimap |
| Esc | Pause menu |

## Multiplayer

Up to four players, co-op against the bot waves:

```
Atlas --host [port]               # host a match (default port 27960)
Atlas --join <address>[:port]     # join one
```

Players can join a match in progress. Other players show with their
own tint and a `P2`/`P3`/`P4` tag; the HUD shows the connection status
under the bots button. The host controls the bots toggle.

How it works: **deterministic lockstep**, the model Factorio and most
RTS games use for worlds with too much state to stream. Every peer runs
the same `World`; only inputs cross the network. Each tick the host
seals everyone's latest input into a `TickFrame`, applies it, and sends
it to all clients, who apply the same frames in the same order, so
dug terrain, debris, AI and particles cost no bandwidth at all. Every
second the host sends a state hash, and a client that disagrees shows
`DESYNC`. A late joiner receives the frame history and fast-forwards the
match from tick 0 before taking control. Transport is
[ENet](https://github.com/lsalzman/enet) (reliable UDP, vendored in
`ThirdParty/enet`), wrapped by `Net/Session`; the wire format is in
`Net/Protocol`.

Current limits: clients feel one round trip of input delay on their
own actor (no client-side prediction yet), all peers should run the
same build on the same OS (different compilers' math libraries can
round differently, which shows up as a desync), and a disconnected
player's body stays in the match idle.

## Architecture

The game is split into a **simulation** and a **client**:

- `Engine/Sim/World` owns all game state (terrain, actors, particles,
  grenades, deliveries, pickups, waves) and advances it in fixed 120 Hz
  ticks with `World::Step(inputs)`. It never touches a window, renderer,
  audio device or the keyboard, so it runs unchanged in the game, in
  headless tests, and later on a dedicated server.
- Players drive their actors only through `PlayerInput` (move, jump,
  jet, crouch, aim, fire, throw, reload, weapon slot) and buy through
  `OrderRequest`, which the World prices itself. Up to four player slots
  exist today; the local game uses slot 0.
- What happened during a tick comes back as `SimEvent`s (sounds, hard
  landings, explosions, kills, damage numbers, wave starts). The client
  (`Core/Application`) turns those into audio, camera shake, hit-stop
  and floating text.
- Every actor has a stable id (`Actor::GetId`); bullets remember their shooter by
  id and team rather than by pointer, so a shooter dying mid-flight is
  safe.
- All simulation randomness comes from the world seed (plus fixed
  per-system seeds). Same seed + same inputs = bit-identical state,
  which `World::ComputeStateHash()` checks. This is the groundwork for
  networking (input replication, replays, desync detection).

## Assets

The art and sound live in `Assets/`, and both packs are built from
source by scripts you can edit and re-run:

- `Tools/art/make_sprites.py` (Pillow) holds every sprite as
  hand-authored pixel art in palette-letter grids, adds outlines, and
  writes 24-bit magenta-keyed BMPs plus `Assets/Sprites/sprites.ini`
  (each sprite's file and pivot: the pixel it is placed and rotated
  around; a weapon's pivot is its grip, with the muzzle `BarrelLength`
  pixels ahead). `--preview sheet.png` writes a contact sheet. The
  engine's `SpriteBank` loads the pack by name; a weapon picks its art
  from its kind and stats, or from a `Sprite = name` key in
  `weapons.ini`.
- `Tools/audio/make_sfx.py` (numpy) synthesizes every effect and writes
  16-bit mono WAVs to `Assets/Sounds`. An effect with several takes is
  saved as `name_1.wav`, `name_2.wav`, ... and the game plays a
  different one each time. Drop in your own WAVs with the same names to
  replace any sound.

`AssetTests` (run by `ctest`) checks that everything listed loads and
that every weapon, build and sound effect has its asset.

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

### Tests

`Tests/NetTests.cpp` plays a real networked match over loopback: a host
and two clients (one joining mid-match) driving scripted input, checking
that every peer ends on the same tick with the same state hash, and that
a fifth player is refused. `Tests/SimTests.cpp` runs the simulation
headless: a determinism check
(two worlds, same seed and scripted inputs, hashes compared every half
second over a simulated minute), an order/ids check, and a five-minute
four-player soak. Run them with:

```
ctest --test-dir build --output-on-failure
```

CI (`.github/workflows/ci.yml`) builds and tests on Linux (Release, and
Debug with AddressSanitizer + UBSan) and on Windows with MSVC.

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
- [x] **Simulation core**: game state pulled out of the game loop into a
  headless `World`, per-player input commands, stable actor ids, seeded
  deterministic simulation, headless tests and CI
- [x] **Networking**: deterministic lockstep co-op for 2-4 players over
  ENet, late join by replay, desync detection
- [x] **Living terrain**: data-driven materials, falling sand, liquids,
  lava/water reactions, caves and pools, bedrock
- [x] **Core loop**: brains and bunkers, enemy drop ships and attack
  waves, body switching, squad orders, team funds, victory and defeat
- [x] **Menus**: main menu, match setup, settings, pause, results
- [ ] **Networking, next**: client-side prediction (or rollback) to hide
  input delay, snapshot-based join for long matches, reconnects,
  cross-platform float determinism
- [ ] **Milestone 4 — game layer**: scenes and props from data files,
  inventory/pickups, objectives. The buy menu is now a real order cart
  (bodies, weapons/tools and supply crates, with weapons equippable to
  a chosen body or dropped loose as field pickups); still to come:
  direct control over multiple bodies.
- [ ] **Milestone 5 — feel and polish**: body pitch and stagger,
  screen shake, parallax background, performance pass, settings
