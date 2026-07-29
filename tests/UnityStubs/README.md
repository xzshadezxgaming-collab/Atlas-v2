# UnityStubs

**Not the real Unity API.** This is a hand-written, best-effort
approximation of the subset of `UnityEngine`/`UnityEditor` types that
`Assets/Scripts/Gameplay` and `Assets/Editor` actually use, written
because no real Unity Editor/DLLs were available in the environment
this project was built in.

Purpose: let `dotnet build` catch real bugs in the Gameplay/Editor
layer (typos, wrong signatures, missing usings, type mismatches) that
would otherwise sit completely unverified. See
`tests/StrainEmpire.Gameplay.CompileCheck/`.

What this does **not** prove:
- That the real Unity compiler accepts the code (stub signatures are
  written from memory/training knowledge of the Unity API, not copied
  from actual Unity DLLs — they could be subtly wrong)
- That anything behaves correctly at runtime (no real MonoBehaviour
  lifecycle, no real Canvas/UI layout, no real Editor scene
  serialization)
- Anything about code gated behind `#if STEAMWORKS_NET` /
  `FIREBASE_ENABLED` / `UNITY_ADS_ENABLED` / `UNITY_IAP_ENABLED` —
  those blocks aren't compiled here either, same as they wouldn't be
  in a real build without those symbols defined

Delete this whole `tests/UnityStubs` and
`tests/StrainEmpire.Gameplay.CompileCheck` directory once the project
has actually been opened in a real Unity Editor — at that point the
real compiler is strictly better than this approximation.
