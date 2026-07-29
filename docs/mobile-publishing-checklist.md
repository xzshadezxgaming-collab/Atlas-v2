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

## Blocked on your developer accounts (I can't do these)

1. **Apple Developer Program** ($99/year) and **Google Play Console**
   developer account ($25 one-time) — real business transactions
   requiring your identity/payment info.
2. **Create the app listings**: bundle ID / package name registration,
   App Store Connect and Play Console app records.
3. **Firebase project setup**: create a Firebase project, add iOS and
   Android apps to it, download `GoogleService-Info.plist` /
   `google-services.json`, import the Firebase Unity SDK (Auth +
   Realtime Database), and define `FIREBASE_ENABLED` in Player
   Settings — see `docs/unity-project-notes.md` "Mobile integration."
4. **Ad network setup**: create a Unity LevelPlay/Ads (or AdMob)
   account, register ad units matching the placeholder IDs in
   `UnityAdsRewardedService` (`Rewarded_Android`, `Rewarded_iOS`) or
   update the code to match whatever IDs your ad account actually
   issues, import the ad SDK, define `UNITY_ADS_ENABLED`.
5. **IAP setup**: import Unity IAP, define `UNITY_IAP_ENABLED`, and
   create the three Gem-pack products in both stores with IDs matching
   `GemPackCatalog` exactly (`gems_small`, `gems_medium`,
   `gems_large`) — pricing is set per-store, not in code.
6. **Content-policy risk — read this before investing in art/marketing**:
   both Apple's App Store Review Guidelines and Google Play's
   Developer Program Policy are meaningfully stricter about drug-
   themed content than Steam, and neither has Steam's precedent
   (*Schedule I*, *Weedcraft Inc*). This needs an actual read of the
   *current* guidelines on both platforms before committing further —
   they change, and a summary here would go stale. Depending on what
   they say, consider softening real-world framing further for the
   mobile listing specifically (fictional substance names, avoid any
   real-drug branding/imagery) even beyond what `docs/store-page-copy.md`
   already does. This is a real go/no-go decision, not a formality —
   worth resolving before the art pass, not after.
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
   IARC content rating questionnaire, both informed by the same
   content-policy read in step 6.
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
   and `Assets/Editor` — treat a clean first compile as unlikely, not
   guaranteed (see `docs/unity-project-notes.md` "What's unverified").
4. Run **Strain Empire > Create MVP Scene**, press Play, and confirm
   the plant → grow → harvest → sell → breed → Gems/Instant Grow loop
   actually works end-to-end with real Editor timing.
5. Only after that: start the art pass, the content-policy read
   (step 6 above), and the developer-account/SDK steps.

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
