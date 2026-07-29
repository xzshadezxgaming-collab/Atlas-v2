namespace StrainEmpire.Gameplay.Ads
{
    public interface IAdService
    {
        bool IsRewardedAdReady { get; }

        /// Invokes onRewardGranted only if the ad was actually watched to
        /// completion (never on skip/failure).
        void ShowRewardedAd(System.Action onRewardGranted);
    }
}
