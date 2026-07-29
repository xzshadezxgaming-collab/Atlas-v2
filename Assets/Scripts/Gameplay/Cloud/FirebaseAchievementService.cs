#if FIREBASE_ENABLED
using Firebase.Database;
using Firebase.Auth;
using UnityEngine;

namespace StrainEmpire.Gameplay.Cloud
{
    /// Records achievement unlocks under the signed-in user's Firebase
    /// record. Unlike Steam Stats/Achievements, Firebase has no built-in
    /// achievement UI — this just persists the unlock; showing it to the
    /// player (toast/popup) is a Gameplay-side concern, not this service's.
    public class FirebaseAchievementService : IAchievementService
    {
        public void Unlock(string apiName)
        {
            FirebaseUser user = FirebaseAuth.DefaultInstance.CurrentUser;
            if (user == null)
            {
                Debug.LogWarning("[FirebaseAchievementService] No signed-in user; achievement not recorded.");
                return;
            }

            FirebaseDatabase.DefaultInstance
                .GetReference("achievements")
                .Child(user.UserId)
                .Child(apiName)
                .SetValueAsync(true);
        }
    }
}
#endif
