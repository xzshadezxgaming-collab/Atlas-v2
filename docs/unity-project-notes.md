# Unity Project Notes

## Opening the project

This repo root *is* the Unity project — `Assets/`, `Packages/`, and
`ProjectSettings/` are all at the top level. Point Unity Hub at the
repo root directly.

Target editor version: **2022.3.20f1** (2022 LTS), pinned in
`ProjectSettings/ProjectVersion.txt`. Any 2022.3 LTS patch should open
it fine; Unity will prompt to re-serialize if the exact patch differs.

This project has **not yet been opened in the Unity Editor** — it was
scaffolded by hand in an environment without Unity installed. First
open will auto-generate `Library/`, `Temp/`, and other Editor-managed
folders (already gitignored) and should just work, since everything
under `Assets/Scripts/` is plain, syntactically-valid C#. The one
thing an Editor pass will surface that this environment couldn't
check: any package-version mismatches in `Packages/manifest.json`
against whatever registry the Editor resolves against.

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
      Economy/      # EconomyConfig, EmpireValueCalculator
      Random/       # IRandomSource + SystemRandomSource (injectable RNG)
      GrowPlot.cs
    Gameplay/       # MonoBehaviours wiring Core to the scene.
                    # Deliberately minimal right now — real wiring
                    # (grow plots, mixing UI, market UI) lands with
                    # the "core grow/mix/sell loop" milestone.
  Scenes/           # Empty for now — create the MVP scene in-Editor
                    # (File > New Scene) when wiring gameplay, rather
                    # than hand-authoring Unity's YAML scene format
                    # here with no way to verify it parses correctly.
Packages/manifest.json
ProjectSettings/ProjectVersion.txt
tests/StrainEmpire.Core.Tests/   # dotnet test project, see below
```

## Why Core has no UnityEngine dependency

Every formula in `docs/systems-design.md` (breeding/mutation,
ingredient mixing, market pricing, plot growth timing, Empire Value)
lives in `Assets/Scripts/Core` as plain C# with randomness injected
via `IRandomSource`. That means it compiles and runs under a normal
`dotnet` SDK with no Unity install at all — which is exactly how it
was verified in this session (no Unity Editor was available):

```
cd tests/StrainEmpire.Core.Tests
dotnet test
```

25 tests currently pass, covering: breeding averaging + mutation +
clamping, ingredient mix deltas + clamping + the 3-slot cap, market
demand tables + the sell-price formula, plot maturity timing, and the
Empire Value rollup. The test project compiles the real
`Assets/Scripts/Core/**/*.cs` files directly (see the `.csproj`), so
it's testing the shipped code, not a copy.

The `Gameplay` assembly (MonoBehaviours) does depend on UnityEngine
and was **not** compiled or verified in this session — there's no way
to do that without the Editor or its DLLs. It's currently just a
`GameManager` stub, so the risk is low, but treat it as unverified
until it's opened in-Editor.
