namespace StrainEmpire.Gameplay.Steam
{
    public static class AchievementServiceFactory
    {
        public static IAchievementService Create()
        {
#if STEAMWORKS_NET
            return new SteamAchievementService();
#else
            return new LocalAchievementService();
#endif
        }
    }
}
