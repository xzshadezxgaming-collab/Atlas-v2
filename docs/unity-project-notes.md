# Unity Project Notes

## Opening the project

This repo root *is* the Unity project — `Assets/`, `Packages/`, and
`ProjectSettings/` are all at the top level. Point Unity Hub at the
repo root directly.

Target editor version: **2022.3.20f1** (2022 LTS), pinned in
`ProjectSettings/ProjectVersion.txt`. Any 2022.3 LTS patch should open
it fine; Unity will prompt to re-serialize if the exact patch differs.

This project has **not yet been opened in the Unity Editor** — it was
built by hand in an environment without Unity installed (no GUI, no
Editor binary available). First open will auto-generate `Library/`,
`Temp/`, and other Editor-managed folders (already gitignored). After
opening, use the **Strain Empire > Create MVP Scene** menu item (added
by `Assets/Editor/SceneBootstrapper.cs`) to generate the first playable
scene — deliberately done this way instead of hand-authoring a `.unity`
YAML file, since Unity itself writes that file when the menu command
runs, removing any risk of a malformed hand-rolled scene.

## Layout

```
Assets/
  Scripts/
    Core/          # Pure C# simulation — no UnityEngine dependency
                    # (asmdef sets noEngineReferences: true), so it's
                    # portable and unit-testable outside the Editor.
      Genetics/     # Strain, BreedingSystem
      Mixing/       # Ingredient, IngredientDatabase, MixingSystem
      Market/       # SeasonArchetype, MarketSystem
      Economy/      # EconomyConfig, EmpireValueCalculator, OfflineProgressCalculator
      Session/      # GameSession — orchestrates plots/cash/strains/season,
                    # exposes the actual player actions. Still pure C#.
      Random/       # IRandomSource + SystemRandomSource (injectable RNG)
      GrowPlot.cs
    Gameplay/       # MonoBehaviours + UnityEngine-dependent wiring.
      GameManager.cs      # Scene entry point: owns a GameSession, ticks it
                           # against real time, builds a runtime UI, wires
                           # save/load + Steam submission.
      UI/RuntimeUIBuilder.cs  # Builds Canvas/Text/Button entirely from code
                           # (programmer art) — no prefabs, so nothing here
                           # depends on a hand-authored binary asset either.
      Save/                # SaveData DTOs + SaveSystem (JsonUtility, local
                           # file under Application.persistentDataPath).
      Steam/                # ILeaderboardService / IAchievementService with
                           # Local* fallbacks and Steam* implementations
                           # guarded by `#if STEAMWORKS_NET` — see
                           # "Steam integration" below.
  Editor/
    SceneBootstrapper.cs  # Strain Empire > Create MVP Scene menu command.
  Scenes/           # Empty until the menu command above is run in-Editor.
Packages/manifest.json
ProjectSettings/ProjectVersion.txt
tests/StrainEmpire.Core.Tests/   # dotnet test project, see below
docs/
  systems-design.md              # formulas this code implements
  steam-publishing-checklist.md  # what's left that needs your Steamworks account
```

## Why Core (and GameSession) has no UnityEngine dependency

Every formula in `docs/systems-design.md` — breeding/mutation,
ingredient mixing, market pricing, plot growth timing, offline catch-up,
Empire Value — plus the `GameSession` class that ties them into actual
player actions (plant, buy plot, harvest+mix+sell, breed, advance
season, save/restore) lives in `Assets/Scripts/Core` as plain C# with
randomness injected via `IRandomSource`. That means it compiles and
runs under a normal `dotnet` SDK with no Unity install at all — which
is how it was verified in this session (no Unity Editor was available):

```
cd tests/StrainEmpire.Core.Tests
dotnet test
```

36 tests currently pass, covering every formula individually, the full
`GameSession` action surface, save/restore round-tripping, and a
30-day headless economy simulation that plays the whole loop
end-to-end and asserts the economy grows sanely — the closest
available substitute for playtesting without an Editor. The test
project compiles the real `Assets/Scripts/Core/**/*.cs` files directly
(see the `.csproj`), so it's testing the shipped code, not a copy.

## What's unverified

`Assets/Scripts/Gameplay/**` and `Assets/Editor/**` depend on
UnityEngine/UnityEditor and were **not** compiled or run in this
session — there is no way to do that without the Editor or its DLLs.
This is real, non-trivial code (a runtime-built UI, save/load, Steam
service wrappers, an Editor menu command), written carefully and kept
as simple as reasonably possible to limit risk, but **the first thing
to do on actually opening this in Unity is fix whatever the Editor's
compiler flags** — treat a clean first compile as unlikely, not a
given. The current UI (`RuntimeUIBuilder`) is deliberately
"programmer art": functional text/button layout, not real UI/UX — it
exists so the loop is playable end-to-end, not as final presentation.

## Steam integration

`Assets/Scripts/Gameplay/Steam/` implements leaderboard submission and
achievement unlocking behind an interface (`ILeaderboardService`,
`IAchievementService`) with two implementations each:

- `Local*Service` — always compiles, logs to the console instead of
  talking to Steam. This is what the project uses **by default**.
- `Steam*Service` — the real Steamworks.NET calls, wrapped in
  `#if STEAMWORKS_NET` so it's entirely excluded from compilation
  unless that symbol is defined.

The Steamworks.NET plugin itself (native binaries + C# bindings) is a
third-party download, not something to vendor into this repo blind.
To turn on real Steam integration:

1. Download Steamworks.NET from its GitHub releases and import the
   `.unitypackage` (it includes the `SteamManager` helper the
   `Steam*Service` classes expect).
2. Project Settings > Player > Scripting Define Symbols: add
   `STEAMWORKS_NET`.
3. Drop your real Steam App ID into `steam_appid.txt` at the project
   root for local testing (see Steamworks.NET's own setup docs).
4. Create the two leaderboards used in code (`season_empire_value`,
   `alltime_empire_value`) and any achievement API names in the
   Steamworks partner site (App Admin > Stats & Achievements).

See `docs/steam-publishing-checklist.md` for everything else needed to
actually publish (App ID, fee, store page, build upload).
