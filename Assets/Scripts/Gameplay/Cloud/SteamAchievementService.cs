#if STEAMWORKS_NET
using Steamworks;
using UnityEngine;

namespace StrainEmpire.Gameplay.Cloud
{
    public class SteamAchievementService : IAchievementService
    {
        public void Unlock(string apiName)
        {
            if (!SteamManager.Initialized)
            {
                Debug.LogWarning("[SteamAchievementService] Steam is not initialized; achievement not unlocked.");
                return;
            }

            SteamUserStats.SetAchievement(apiName);
            SteamUserStats.StoreStats();
        }
    }
}
#endif
