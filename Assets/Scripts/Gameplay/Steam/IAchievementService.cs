namespace StrainEmpire.Gameplay.Steam
{
    public interface IAchievementService
    {
        /// apiName must match an achievement configured in the Steamworks
        /// partner site (App Admin > Stats & Achievements).
        void Unlock(string apiName);
    }
}
