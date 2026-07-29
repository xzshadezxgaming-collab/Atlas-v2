# Steam Publishing Checklist

Honest split of what's actually finished in this repo versus what
needs a human with a Steamworks account, a credit card, and (for art)
creative tools — none of which an agent working in a headless
environment with no Unity Editor, no Steam account, and no image/audio
generation tooling can complete. This doc exists so "ready for
publishing" has a concrete, checkable meaning instead of staying vague.

## Done in this repo

- [x] Game concept, full systems design with concrete formulas
      (`GAME_CONCEPT.md`, `docs/systems-design.md`)
- [x] Core simulation logic — breeding/genetics, ingredient mixing,
      seasonal market pricing, plot growth, offline catch-up, Empire
      Value — implemented and unit tested (36/36 passing,
      `tests/StrainEmpire.Core.Tests`)
- [x] Full game-action layer (`GameSession`): plant, buy plot,
      harvest+mix+sell, breed, advance season, save/restore — also
      unit tested, including a 30-day headless economy simulation
      standing in for a playtest
- [x] Unity project scaffold (`Assets/`, `Packages/`,
      `ProjectSettings/`), MVP scene creation via an Editor menu
      command (`Strain Empire > Create MVP Scene`)
- [x] Gameplay wiring: `GameManager` ticking the session against real
      time, a functional (placeholder-art) runtime UI, local save/load
      with offline progress on relaunch
- [x] Steam integration code: leaderboard submission and achievement
      unlocking behind an interface, with a working local fallback and
      a `#if STEAMWORKS_NET`-guarded real implementation ready to
      activate once the plugin is imported

## Blocked on your Steamworks account (I can't do these)

1. **Create a Steamworks partner account and pay the $100 App
   registration fee** at partner.steamgames.com. This is a real
   business transaction requiring your identity/payment info.
2. **Get your App ID**, generate `steam_appid.txt` for local testing,
   and create the two leaderboards this code already calls
   (`season_empire_value`, `alltime_empire_value`) plus any
   achievement API names, under App Admin > Stats & Achievements.
3. **Import Steamworks.NET** into the Unity project (see
   `docs/unity-project-notes.md` "Steam integration" for the exact
   steps) and define `STEAMWORKS_NET` in Player Settings to switch
   from the local fallback to real Steam calls.
4. **Tax and banking info** in the Steamworks partner site — required
   before Valve will pay out revenue.
5. **Store page**: description copy is drafted (see
   `docs/store-page-copy.md`), but capsule images, header art,
   screenshots, and a trailer must show *real, running gameplay* —
   Valve requires this and will reject placeholder/mockup art. That
   means a genuine art pass (hire/commission an artist, buy a fitting
   UI/asset pack, or DIY) has to happen before the store page can go
   live, since the current UI is deliberately programmer-art.
6. **Content rating**: run Steam's rating questionnaire (IARC) once
   the store page is being set up. Given the cultivation/dealing theme,
   expect an adult-ish rating band — this is normal and has precedent
   (*Schedule I*, *Weedcraft Inc*, *Drug Dealer Simulator* all shipped
   under Steam's existing content policy for this theme), not a
   blocker, just something to answer honestly in the questionnaire.
7. **Build and upload**: export a Windows/Mac/Linux build from the
   Unity Editor, then upload via SteamPipe (`steamcmd` +
   `app_build.vdf`/depot scripts) — needs your App ID and Steamworks
   credentials, can't be scripted from here without them.
8. **"Coming Soon" page**: Valve requires the store page to be live at
   least ~2 weeks before release (their current guidance — check
   partner docs for the exact window at the time you publish).
9. **Submit for review** and set a release date once Valve approves.

## First Unity Editor session — do this before anything else above

Since no Unity Editor was available while building this, the very
first thing to do on opening the project is:

1. Let Unity import and resolve `Packages/manifest.json`.
2. Fix whatever compile errors surface in `Assets/Scripts/Gameplay`
   and `Assets/Editor` — treat a clean first compile as unlikely, not
   guaranteed (see `docs/unity-project-notes.md` "What's unverified").
3. Run **Strain Empire > Create MVP Scene**, press Play, and confirm
   the plant → grow → harvest → sell → breed loop actually works
   end-to-end with real Editor timing (not just the dotnet simulation).
4. Only after that: start the art pass and the Steamworks account
   steps above.

## Not blocking a first release, but worth planning

- Real UI/UX art pass (see above — actually required for the store
  page, so this is more "next" than "later")
- Ingredient-selection UI (harvesting currently auto-sells with no
  ingredients — the mixing system is fully implemented and tested in
  Core, it just has no UI hook yet)
- Sound/music
- Steam Cloud explicit config (local save file is already
  Cloud-sync-compatible if you point Steam's folder-based sync at
  `Application.persistentDataPath`)
- Balance tuning once real players are on it — the numbers in
  `docs/systems-design.md` are a deliberate first pass, not
  final-tuned
