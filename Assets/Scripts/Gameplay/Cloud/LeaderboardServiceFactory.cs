namespace StrainEmpire.Gameplay.Cloud
{
    public static class LeaderboardServiceFactory
    {
        /// Firebase is the mobile-primary path; Steam stays available for a
        /// possible PC release. Neither being configured (the out-of-the-box
        /// state) falls back to the local no-op so the project always builds.
        public static ILeaderboardService Create()
        {
#if FIREBASE_ENABLED
            return new FirebaseLeaderboardService();
#elif STEAMWORKS_NET
            return new SteamLeaderboardService();
#else
            return new LocalLeaderboardService();
#endif
        }
    }
}
