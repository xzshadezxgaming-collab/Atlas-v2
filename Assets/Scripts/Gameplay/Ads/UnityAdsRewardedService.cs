#if UNITY_ADS_ENABLED
using UnityEngine;
using UnityEngine.Advertisements;

namespace StrainEmpire.Gameplay.Ads
{
    /// Real rewarded-ad implementation via Unity Ads/LevelPlay. Only
    /// compiled once the package is imported and UNITY_ADS_ENABLED is
    /// defined (Project Settings > Player > Scripting Define Symbols).
    /// Ad SDK APIs change across package versions more than most - this
    /// targets the classic UnityEngine.Advertisements surface
    /// (IUnityAdsLoadListener/IUnityAdsShowListener); check it against
    /// whichever package version actually gets installed before relying on
    /// it. See docs/mobile-publishing-checklist.md.
    public class UnityAdsRewardedService : IAdService, IUnityAdsLoadListener, IUnityAdsShowListener
    {
        private const string AndroidAdUnitId = "Rewarded_Android";
        private const string IosAdUnitId = "Rewarded_iOS";

#if UNITY_IOS
        private static string AdUnitId => IosAdUnitId;
#else
        private static string AdUnitId => AndroidAdUnitId;
#endif

        private System.Action _pendingReward;
        private bool _isLoaded;

        public bool IsRewardedAdReady => _isLoaded;

        public void Load() => Advertisement.Load(AdUnitId, this);

        public void ShowRewardedAd(System.Action onRewardGranted)
        {
            if (!_isLoaded)
            {
                Debug.LogWarning("[UnityAdsRewardedService] Ad not loaded yet.");
                return;
            }

            _pendingReward = onRewardGranted;
            Advertisement.Show(AdUnitId, this);
        }

        public void OnUnityAdsAdLoaded(string adUnitId) => _isLoaded = true;

        public void OnUnityAdsFailedToLoad(string adUnitId, UnityAdsLoadError error, string message) => _isLoaded = false;

        public void OnUnityAdsShowFailure(string adUnitId, UnityAdsShowError error, string message)
        {
            _pendingReward = null;
            _isLoaded = false;
            Load();
        }

        public void OnUnityAdsShowStart(string adUnitId) { }

        public void OnUnityAdsShowClick(string adUnitId) { }

        public void OnUnityAdsShowComplete(string adUnitId, UnityAdsShowCompletionState showCompletionState)
        {
            _isLoaded = false;
            Load(); // pre-load the next one

            if (showCompletionState == UnityAdsShowCompletionState.COMPLETED)
                _pendingReward?.Invoke();

            _pendingReward = null;
        }
    }
}
#endif
