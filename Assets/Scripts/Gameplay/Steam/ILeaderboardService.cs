namespace StrainEmpire.Gameplay.Steam
{
    /// Abstraction over Steam Leaderboards so GameManager never needs to
    /// know whether the Steamworks.NET plugin is actually installed.
    public interface ILeaderboardService
    {
        void SubmitScore(string leaderboardName, int score);
    }
}
