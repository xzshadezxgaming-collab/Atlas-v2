namespace StrainEmpire.Gameplay.Store
{
    public interface ICurrencyStoreService
    {
        void Purchase(GemPack pack, System.Action<int> onGemsGranted);
    }
}
