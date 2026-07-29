using UnityEngine;

namespace StrainEmpire.Gameplay.Cloud
{
    /// Fallback used until the Steamworks.NET plugin is imported and the
    /// STEAMWORKS_NET scripting define symbol is set (Project Settings >
    /// Player > Scripting Define Symbols). See docs/steam-publishing-checklist.md.
    /// Keeps the game fully playable/buildable without the plugin present.
    public class LocalLeaderboardService : ILeaderboardService
    {
        public void SubmitScore(string leaderboardName, int score)
        {
            Debug.Log($"[LocalLeaderboardService] Would submit {score} to '{leaderboardName}' (Steamworks.NET not installed).");
        }
    }
}
