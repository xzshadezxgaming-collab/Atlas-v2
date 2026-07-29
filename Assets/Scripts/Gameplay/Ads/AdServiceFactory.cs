namespace StrainEmpire.Gameplay.Ads
{
    public static class AdServiceFactory
    {
        public static IAdService Create()
        {
#if UNITY_ADS_ENABLED
            return new UnityAdsRewardedService();
#else
            return new LocalAdService();
#endif
        }
    }
}
