namespace StrainEmpire.Gameplay.Cloud
{
    public static class AchievementServiceFactory
    {
        public static IAchievementService Create()
        {
#if FIREBASE_ENABLED
            return new FirebaseAchievementService();
#elif STEAMWORKS_NET
            return new SteamAchievementService();
#else
            return new LocalAchievementService();
#endif
        }
    }
}
