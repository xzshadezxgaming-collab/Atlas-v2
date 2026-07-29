# Unity Project Notes

## Opening the project

This repo root *is* the Unity project — `Assets/`, `Packages/`, and
`ProjectSettings/` are all at the top level. Point Unity Hub at the
repo root directly.

Target editor version: **2022.3.20f1** (2022 LTS), pinned in
`ProjectSettings/ProjectVersion.txt`. Any 2022.3 LTS patch should open
it fine; Unity will prompt to re-serialize if the exact patch differs.
Build target is mobile (Android/iOS) — the project was scaffolded
without picking one, so switch it in File > Build Settings on first
open.

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
      Session/      # GameSession — orchestrates plots/cash/gems/strains/
                    # season, exposes the actual player actions. Still pure C#.
      Random/       # IRandomSource + SystemRandomSource (injectable RNG)
      GrowPlot.cs
    Gameplay/       # MonoBehaviours + UnityEngine-dependent wiring.
      GameManager.cs      # Scene entry point: owns a GameSession, ticks it
                           # against real time, builds a runtime UI, wires
                           # save/load + ad/IAP/leaderboard submission.
      UI/RuntimeUIBuilder.cs  # Builds Canvas/Text/Button entirely from code
                           # (programmer art) — no prefabs, so nothing here
                           # depends on a hand-authored binary asset either.
      Save/                # SaveData DTOs + SaveSystem (JsonUtility, local
                           # file under Application.persistentDataPath).
      Cloud/                # ILeaderboardService / IAchievementService with
                           # Local* fallbacks, Firebase* (mobile-primary,
                           # `#if FIREBASE_ENABLED`) and Steam* (secondary,
                           # `#if STEAMWORKS_NET`) implementations — see
                           # "Mobile integration" below.
      Ads/                  # IAdService: LocalAdService (default) and
                           # UnityAdsRewardedService (`#if UNITY_ADS_ENABLED`).
      Store/                # ICurrencyStoreService (Gem IAP): GemPackCatalog,
                           # LocalCurrencyStoreService (default) and
                           # UnityIapCurrencyStoreService (`#if UNITY_IAP_ENABLED`).
  Editor/
    SceneBootstrapper.cs  # Strain Empire > Create MVP Scene menu command.
  Scenes/           # Empty until the menu command above is run in-Editor.
Packages/manifest.json
ProjectSettings/ProjectVersion.txt
tests/StrainEmpire.Core.Tests/   # dotnet test project, see below
docs/
  systems-design.md                # formulas this code implements
  mobile-publishing-checklist.md   # what's left that needs your dev accounts
```

## Why Core (and GameSession) has no UnityEngine dependency

Every formula in `docs/systems-design.md` — breeding/mutation,
ingredient mixing, market pricing, plot growth timing, offline
catch-up, Gems/Instant Grow, Empire Value — plus the `GameSession`
class that ties them into actual player actions (plant, buy plot,
harvest+mix+sell, breed, advance season, add Gems, instant-grow,
save/restore) lives in `Assets/Scripts/Core` as plain C# with
randomness injected via `IRandomSource`. That means it compiles and
runs under a normal `dotnet` SDK with no Unity install at all — which
is how it was verified in this session (no Unity Editor was available):

```
cd tests/StrainEmpire.Core.Tests
dotnet test
```

40 tests currently pass, covering every formula individually, the full
`GameSession` action surface (including Gems/Instant Grow), save/
restore round-tripping, and a 30-day headless economy simulation that
plays the whole loop end-to-end and asserts the economy grows sanely —
the closest available substitute for playtesting without an Editor.
The test project compiles the real `Assets/Scripts/Core/**/*.cs` files
directly (see the `.csproj`), so it's testing the shipped code, not a
copy.

## What's unverified

`Assets/Scripts/Gameplay/**` and `Assets/Editor/**` depend on
UnityEngine/UnityEditor and were **not** compiled or run in this
session — there is no way to do that without the Editor or its DLLs.
This is real, non-trivial code (a runtime-built UI, save/load, ad/IAP/
leaderboard service wrappers, an Editor menu command), written
carefully and kept as simple as reasonably possible to limit risk, but
**the first thing to do on actually opening this in Unity is fix
whatever the Editor's compiler flags** — treat a clean first compile
as unlikely, not a given. The current UI (`RuntimeUIBuilder`) is
deliberately "programmer art": functional text/button layout, not real
UI/UX — it exists so the loop is playable end-to-end, not as final
presentation.

Ad/IAP SDK API surfaces in particular shift across package versions
more than most Unity APIs — `UnityAdsRewardedService` and
`UnityIapCurrencyStoreService` are best-effort reference
implementations against a plausible current API shape, explicitly
flagged in their own file comments as needing a version check against
whatever you actually install.

## Mobile integration (Firebase + Ads + IAP)

Mobile has no single unified leaderboard service the way Steam does —
Game Center and Google Play Games Services don't share data with each
other — so `FirebaseLeaderboardService`/`FirebaseAchievementService`
(in `Assets/Scripts/Gameplay/Cloud/`) are the mobile-primary path, one
shared backend for both platforms. Ads and IAP follow the same
guarded-compilation pattern: a `Local*` implementation always compiles
and is what the project uses by default (grants rewards/purchases
immediately — safe for development, must never ship), and the real
implementation is wrapped in `#if SYMBOL_NAME` so it's excluded from
compilation until you define that symbol.

To turn on the real mobile stack:

1. **Firebase**: create a Firebase project, add iOS + Android apps to
   it, import the Firebase Unity SDK (Auth + Realtime Database),
   drop in `GoogleService-Info.plist` / `google-services.json`, define
   `FIREBASE_ENABLED` in Player Settings > Scripting Define Symbols.
2. **Ads**: import Unity LevelPlay/Ads (or swap in AdMob if preferred
   — `IAdService` is the abstraction point either way), register ad
   units, update the placeholder IDs in `UnityAdsRewardedService`
   (`Rewarded_Android`, `Rewarded_iOS`) to match, define
   `UNITY_ADS_ENABLED`.
3. **IAP**: import Unity IAP, create the three Gem-pack products in
   both stores with IDs matching `GemPackCatalog` exactly
   (`gems_small`, `gems_medium`, `gems_large`), define
   `UNITY_IAP_ENABLED`.

See `docs/mobile-publishing-checklist.md` for everything else needed
to actually publish (developer accounts, fees, privacy compliance,
content-policy risk, store listing, build submission).

## Steam integration (secondary — kept for a possible future PC release)

`Assets/Scripts/Gameplay/Cloud/SteamLeaderboardService.cs` and
`SteamAchievementService.cs` still exist behind `#if STEAMWORKS_NET`,
unused by default. If a Steam release ever happens alongside mobile:
import Steamworks.NET (includes the `SteamManager` helper these
classes expect), define `STEAMWORKS_NET`, create a `steam_appid.txt`
for local testing, and create the same leaderboard/achievement names
in the Steamworks partner site. `LeaderboardServiceFactory`/
`AchievementServiceFactory` already prefer Firebase first, so both
symbols can be defined at once without conflict — Steam just won't be
used unless Firebase isn't configured.
