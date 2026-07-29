namespace StrainEmpire.Gameplay.Steam
{
    public static class LeaderboardServiceFactory
    {
        public static ILeaderboardService Create()
        {
#if STEAMWORKS_NET
            return new SteamLeaderboardService();
#else
            return new LocalLeaderboardService();
#endif
        }
    }
}
