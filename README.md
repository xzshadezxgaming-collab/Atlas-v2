# Strain Empire

An idle cultivation empire mobile game: breed strains, mix ingredients
to match a shifting seasonal market, and climb a weekly Empire Value
leaderboard. Optional rewarded ads and buyable Gems only ever
compress time — never pay-to-win — see `GAME_CONCEPT.md` for the full
pitch.

## Docs

- [`GAME_CONCEPT.md`](GAME_CONCEPT.md) — pitch, core loop, competitive
  layer, monetization
- [`docs/systems-design.md`](docs/systems-design.md) — concrete
  formulas: breeding/mutation, ingredient mixing, market pricing,
  Gems/speedups, economy balance, leaderboard score
- [`docs/unity-project-notes.md`](docs/unity-project-notes.md) —
  project layout, how to open it, what's verified vs. not
- [`docs/mobile-publishing-checklist.md`](docs/mobile-publishing-checklist.md) —
  what's done vs. what needs real App Store/Play Console accounts to finish
- [`docs/store-page-copy.md`](docs/store-page-copy.md) — draft store
  listing copy, keywords, content-rating notes

## Running the tests

The entire game-logic layer (`Assets/Scripts/Core`) has no UnityEngine
dependency and is covered by a normal dotnet test project:

```
cd tests/StrainEmpire.Core.Tests
dotnet test
```

CI runs this on every push/PR (`.github/workflows/core-tests.yml`).

## Opening in Unity

Point Unity Hub at this repo root (2022.3 LTS). See
`docs/unity-project-notes.md` for the first-open steps, including the
**Strain Empire > Create MVP Scene** menu command.
