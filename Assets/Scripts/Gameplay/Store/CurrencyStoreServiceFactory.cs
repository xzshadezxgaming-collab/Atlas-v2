namespace StrainEmpire.Gameplay.Store
{
    public static class CurrencyStoreServiceFactory
    {
        public static ICurrencyStoreService Create()
        {
#if UNITY_IAP_ENABLED
            return new UnityIapCurrencyStoreService();
#else
            return new LocalCurrencyStoreService();
#endif
        }
    }
}
