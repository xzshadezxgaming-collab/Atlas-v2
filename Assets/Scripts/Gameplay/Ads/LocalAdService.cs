using UnityEngine;

namespace StrainEmpire.Gameplay.Ads
{
    /// Fallback used until a real ad SDK (Unity Ads/LevelPlay, AdMob, etc.)
    /// is imported and UNITY_ADS_ENABLED is defined (Project Settings >
    /// Player > Scripting Define Symbols). Grants the reward immediately so
    /// the Gems/speedup loop is fully testable without a live ad network.
    /// See docs/mobile-publishing-checklist.md.
    public class LocalAdService : IAdService
    {
        public bool IsRewardedAdReady => true;

        public void ShowRewardedAd(System.Action onRewardGranted)
        {
            Debug.Log("[LocalAdService] Simulating rewarded ad watch (no ad SDK installed).");
            onRewardGranted?.Invoke();
        }
    }
}
