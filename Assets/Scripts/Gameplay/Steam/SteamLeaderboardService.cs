#if STEAMWORKS_NET
using Steamworks;
using UnityEngine;

namespace StrainEmpire.Gameplay.Steam
{
    /// Real Steam Leaderboards implementation. Only compiled once
    /// Steamworks.NET is imported and STEAMWORKS_NET is defined (Project
    /// Settings > Player > Scripting Define Symbols). Requires the
    /// SteamManager helper that ships with the Steamworks.NET package to be
    /// present in the scene/project. See docs/steam-publishing-checklist.md.
    public class SteamLeaderboardService : ILeaderboardService
    {
        public void SubmitScore(string leaderboardName, int score)
        {
            if (!SteamManager.Initialized)
            {
                Debug.LogWarning("[SteamLeaderboardService] Steam is not initialized; score not submitted.");
                return;
            }

            SteamAPICall_t handle = SteamUserStats.FindLeaderboard(leaderboardName);
            var callResult = CallResult<LeaderboardFindResult_t>.Create((result, ioFailure) =>
            {
                if (ioFailure || result.m_bLeaderboardFound == 0)
                {
                    Debug.LogWarning($"[SteamLeaderboardService] Could not find leaderboard '{leaderboardName}'.");
                    return;
                }

                SteamUserStats.UploadLeaderboardScore(
                    result.m_hSteamLeaderboard,
                    ELeaderboardUploadScoreMethod.k_ELeaderboardUploadScoreMethodKeepBest,
                    score,
                    null,
                    0);
            });
            callResult.Set(handle);
        }
    }
}
#endif
