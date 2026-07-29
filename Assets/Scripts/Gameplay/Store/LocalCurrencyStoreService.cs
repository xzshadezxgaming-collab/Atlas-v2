using UnityEngine;

namespace StrainEmpire.Gameplay.Store
{
    /// Fallback used until Unity IAP is imported and UNITY_IAP_ENABLED is
    /// defined (Project Settings > Player > Scripting Define Symbols).
    /// Grants Gems immediately with no real transaction, so the loop is
    /// testable without a live store connection. NEVER ship a build with
    /// this active outside development — it's a free-money bug. See
    /// docs/mobile-publishing-checklist.md.
    public class LocalCurrencyStoreService : ICurrencyStoreService
    {
        public void Purchase(GemPack pack, System.Action<int> onGemsGranted)
        {
            Debug.Log($"[LocalCurrencyStoreService] Simulating purchase of {pack.DisplayName} (no IAP SDK installed).");
            onGemsGranted?.Invoke(pack.GemAmount);
        }
    }
}
