# Mobile Publishing Checklist (iOS + Android)

Honest split of what's actually finished in this repo versus what
needs a human with developer accounts, a credit card, and (for art)
creative tools — none of which an agent working in a headless
environment with no Unity Editor, no App Store/Play Console accounts,
and no image/audio generation tooling can complete. This doc
supersedes the earlier Steam-specific checklist now that mobile is the
primary target; the Steam integration code is kept in the repo (see
`docs/unity-project-notes.md`) in case a PC release ever happens
alongside mobile, but it's not the priority path.

## Done in this repo

- [x] Game concept, full systems design with concrete formulas,
      including the Gems/speedup economy (`GAME_CONCEPT.md`,
      `docs/systems-design.md`)
- [x] Core simulation logic — breeding/genetics, ingredient mixing,
      seasonal market pricing, plot growth, offline catch-up, Gems/
      Instant Grow, Empire Value — unit tested (40/40 passing,
      `tests/StrainEmpire.Core.Tests`)
- [x] Full game-action layer (`GameSession`), unit tested including a
      30-day headless economy simulation standing in for a playtest
- [x] Unity project scaffold, MVP scene creation via an Editor menu
      command
- [x] Gameplay wiring: `GameManager` with a functional
      (placeholder-art) runtime UI — plant/harvest/breed, ingredient
      mixing, Watch Ad, Buy Gems, per-plot Instant Grow — local
      save/load with offline progress and daily ad-cap tracking
- [x] Ads/IAP/leaderboard integration code behind interfaces
      (`IAdService`, `ICurrencyStoreService`, `ILeaderboardService`,
      `IAchievementService`), each with a safe local fallback and a
      real implementation guarded by a scripting define symbol —
      ready to activate once the actual SDKs are imported
- [x] Content-policy research (drug theme on mobile stores) — actually
      done, not deferred; see item 6 below
- [x] Gameplay/Editor code now gets a compile check (not a real Unity
      compile — see `docs/unity-project-notes.md` "What's compile-
      checked") instead of sitting completely unverified

## Package name

**`com.strainempire.idlecultivator`** — picked now so it's fixed
before it touches Play Console, Firebase, or Unity Player Settings,
since changing it later means a new app record (Android package names
are immutable once published). This is a naming convention, not a
domain you need to own — completely standard for indie/solo
publishing. If you'd rather use something else, swap it everywhere
before creating the Play Console app record; nothing below depends on
this exact string, just on it being consistent.

## Status

1. ~~Apple Developer Program ($99/year) and Google Play Console
   developer account ($25 one-time)~~ — **Google Play Console: done.**
   Apple Developer Program still needed if/when an iOS release happens
   — not required to ship the Android version first.
2. **Create the Play Console app record** (next concrete step, doable
   now): Play Console > Create app — app name (e.g. "Strain Empire:
   Idle Cultivator"), default language, Game category, Free. Use the
   package name above when it's asked for (usually at first upload,
   not app creation — Play Console derives it from the first AAB
   you upload, so this is really "make sure Unity's Player Settings
   package name matches the value above before your first build/
   upload," not something to type into Play Console directly right
   now).
3. **Firebase project setup**: create a Firebase project, add an
   Android app to it using the package name above, download
   `google-services.json`, import the Firebase Unity SDK (Auth +
   Realtime Database), and define `FIREBASE_ENABLED` in Player
   Settings — see `docs/unity-project-notes.md` "Mobile integration."
   Can be done now with the Google account that's sorted.
4. **Ad network setup**: create a Unity LevelPlay/Ads (or AdMob)
   account, register ad units matching the placeholder IDs in
   `UnityAdsRewardedService` (`Rewarded_Android`, `Rewarded_iOS`) or
   update the code to match whatever IDs your ad account actually
   issues, import the ad SDK, define `UNITY_ADS_ENABLED`.
5. **IAP setup**: import Unity IAP, define `UNITY_IAP_ENABLED`, and
   create the three Gem-pack products in both stores with IDs matching
   `GemPackCatalog` exactly (`gems_small`, `gems_medium`,
   `gems_large`) — pricing is set per-store, not in code.
6. **Content policy — researched, precedented, but has real rules to
   follow.** This was flagged as an open risk in an earlier pass;
   here's the actual finding after reading both platforms' current
   policies and checking live app store listings (as of mid-2026).

   Both platforms explicitly restrict the same two things: **content
   that facilitates a real drug sale/purchase**, and (Google's
   wording specifically) **real instructions for growing or
   manufacturing illegal drugs**. Neither restricts *fictional,
   gamified simulation* of the theme — and this isn't theoretical:
   drug-cultivation/dealing sim games are live *right now* on both
   Google Play and the Apple App Store (e.g. "Drug Dealer Simulator,"
   "Schedule I," "Weed Farm," "Drug Dealer: Grand Mafia Games" —
   several with a 17+/Mature rating and disclosed content descriptors
   like "Alcohol, Tobacco, Drug Use or References"). So the theme
   itself is approvable on mobile, contrary to what I originally
   assumed here — this game's category isn't unprecedented.

   What actually matters for staying on the right side of this:
   - Set the age rating to 17+/Mature and disclose the relevant
     content descriptors honestly in both stores' rating
     questionnaires — every live comparable does this, don't try to
     rate it lower to widen the audience.
   - Keep the game a fictional simulation with abstract numbers
     (which it already is — see `docs/systems-design.md`'s trait/
     ingredient system), not real cultivation/synthesis instructions
     dressed up as gameplay tips.
   - No real transactions: Gems/IAP buy in-game currency only, never
     anything that reads as a real drug purchase — already the case,
     see `GAME_CONCEPT.md` "Monetization."
   - `docs/store-page-copy.md`'s existing avoidance of real-world
     drug/brand references in the listing copy is good practice and
     worth keeping, even though it's not strictly required by what's
     live today.

   This is Apple/Google policy as researched today, not a legal
   opinion — policies and enforcement drift over time, so a final
   read of the current guidelines immediately before submission is
   still worth doing, just as a confirmation rather than a blocking
   unknown. Sources: [App Review Guidelines](https://developer.apple.com/app-store/review/guidelines/),
   [Illegal or Recreational Drugs — Play Console Help](https://support.google.com/googleplay/android-developer/answer/6159991).
7. **Privacy compliance** (both platforms now require this
   explicitly, and ads/IAP make it non-optional here):
   - **Apple**: Privacy Nutrition Labels in App Store Connect, and an
     **App Tracking Transparency (ATT)** prompt if the ad SDK does
     any cross-app tracking — required before showing that prompt or
     using tracking-capable ad identifiers. Also: iOS requires
     **Privacy Manifest files** for third-party SDKs (Firebase, the ad
     SDK, Unity IAP) declaring what data they access — check each
     SDK's own docs for whether they ship one or you need to add it.
   - **Google Play**: the **Data Safety** form in Play Console must
     accurately reflect what Firebase/the ad SDK/IAP collect.
   - **COPPA / Google Play Families**: given the theme, this app
     should be explicitly marked not-directed-at-children on both
     stores — don't opt into any children's category.
8. **Age rating**: Apple's age rating questionnaire and Google Play's
   IARC content rating questionnaire — target 17+/Mature with drug-
   reference content descriptors disclosed, per step 6's findings.
9. **Store listing assets**: app icon, feature graphic (Google Play),
   screenshots for each required device size, and (recommended, not
   always required) a preview video — all need to show *real, running
   gameplay*, which means the art pass (see below) has to happen
   before these can exist.
10. **Build and submit**: export the Android (AAB) and iOS builds from
    the Unity Editor, upload via Play Console / App Store Connect
    (Xcode or Transporter), and submit for review. Review timelines
    and exact requirements shift over time — check current guidance
    on each platform when you get here.

## First Unity Editor session — do this before anything else above

Since no Unity Editor was available while building this, the very
first thing to do on opening the project is:

1. Switch the build target to Android or iOS (File > Build Settings)
   — the project was scaffolded without picking one.
2. Let Unity import and resolve `Packages/manifest.json`.
3. Fix whatever compile errors surface in `Assets/Scripts/Gameplay`
   and `Assets/Editor` — a stub compile-check already passes (see
   `docs/unity-project-notes.md` "What's compile-checked"), but treat
   a clean first *Unity* compile as still unproven, not guaranteed.
4. Run **Strain Empire > Create MVP Scene**, press Play, and confirm
   the plant → grow → harvest → sell → breed → Gems/Instant Grow loop
   actually works end-to-end with real Editor timing.
5. Only after that: start the art pass and the developer-account/SDK
   steps (content policy is already researched — see step 6 above).

## Not blocking a first release, but worth planning

- Real UI/UX art pass — required for store listing screenshots, so
  this is more "next" than "later." The full loop (mixing, breeding,
  Gems, Instant Grow) is already wired (programmer art), so this is a
  visual pass, not new gameplay wiring.
- Sound/music
- Balance tuning once real players are on it — the numbers in
  `docs/systems-design.md`, including the Gems economy, are a
  deliberate first pass, not final-tuned. Watch specifically whether
  Instant Grow pricing feels fair versus how many Gems ad-watching
  realistically yields per day.
- A/B testing the ad cap (currently 5/day) and IAP pricing once real
  usage data exists
