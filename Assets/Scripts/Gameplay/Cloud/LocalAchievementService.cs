using UnityEngine;

namespace StrainEmpire.Gameplay.Cloud
{
    public class LocalAchievementService : IAchievementService
    {
        public void Unlock(string apiName)
        {
            Debug.Log($"[LocalAchievementService] Would unlock '{apiName}' (Steamworks.NET not installed).");
        }
    }
}
