using UnityEngine;

namespace StrainEmpire.Gameplay.Cloud
{
    /// Fallback used until a real cloud leaderboard backend (Firebase by
    /// default — see docs/unity-project-notes.md "Mobile integration") is
    /// configured and its scripting define symbol is set. Keeps the game
    /// fully playable/buildable without any backend present.
    public class LocalLeaderboardService : ILeaderboardService
    {
        public void SubmitScore(string leaderboardName, int score)
        {
            Debug.Log($"[LocalLeaderboardService] Would submit {score} to '{leaderboardName}' (no cloud backend configured).");
        }
    }
}
