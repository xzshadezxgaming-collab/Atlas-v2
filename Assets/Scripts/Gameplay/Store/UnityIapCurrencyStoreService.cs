#if UNITY_IAP_ENABLED
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Purchasing;

namespace StrainEmpire.Gameplay.Store
{
    /// Real IAP implementation via Unity IAP. Only compiled once the
    /// package is imported and UNITY_IAP_ENABLED is defined (Project
    /// Settings > Player > Scripting Define Symbols). Products must be
    /// configured in App Store Connect / Play Console with IDs matching
    /// GemPackCatalog exactly. The IStoreListener surface has changed
    /// slightly across Unity IAP versions — check this against whichever
    /// version actually gets installed. See docs/mobile-publishing-checklist.md.
    public class UnityIapCurrencyStoreService : ICurrencyStoreService, IStoreListener
    {
        private IStoreController _storeController;
        private readonly Dictionary<string, System.Action<int>> _pendingCallbacks = new Dictionary<string, System.Action<int>>();

        public UnityIapCurrencyStoreService()
        {
            var builder = ConfigurationBuilder.Instance(StandardPurchasingModule.Instance());
            foreach (GemPack pack in GemPackCatalog.All)
                builder.AddProduct(pack.ProductId, ProductType.Consumable);

            UnityPurchasing.Initialize(this, builder);
        }

        public void Purchase(GemPack pack, System.Action<int> onGemsGranted)
        {
            if (_storeController == null)
            {
                Debug.LogWarning("[UnityIapCurrencyStoreService] Store not initialized yet.");
                return;
            }

            _pendingCallbacks[pack.ProductId] = onGemsGranted;
            _storeController.InitiatePurchase(pack.ProductId);
        }

        public void OnInitialized(IStoreController controller, IExtensionProvider extensions) => _storeController = controller;

        public void OnInitializeFailed(InitializationFailureReason error) =>
            Debug.LogWarning($"[UnityIapCurrencyStoreService] Init failed: {error}");

        public void OnInitializeFailed(InitializationFailureReason error, string message) =>
            Debug.LogWarning($"[UnityIapCurrencyStoreService] Init failed: {error} - {message}");

        public PurchaseProcessingResult ProcessPurchase(PurchaseEventArgs args)
        {
            string productId = args.purchasedProduct.definition.id;
            int gemAmount = 0;
            foreach (GemPack pack in GemPackCatalog.All)
            {
                if (pack.ProductId == productId)
                {
                    gemAmount = pack.GemAmount;
                    break;
                }
            }

            if (_pendingCallbacks.TryGetValue(productId, out System.Action<int> callback))
            {
                callback?.Invoke(gemAmount);
                _pendingCallbacks.Remove(productId);
            }

            return PurchaseProcessingResult.Complete;
        }

        public void OnPurchaseFailed(Product product, PurchaseFailureReason reason)
        {
            _pendingCallbacks.Remove(product.definition.id);
            Debug.LogWarning($"[UnityIapCurrencyStoreService] Purchase failed: {reason}");
        }
    }
}
#endif
