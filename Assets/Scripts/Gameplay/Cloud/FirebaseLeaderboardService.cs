#if FIREBASE_ENABLED
using Firebase.Database;
using Firebase.Auth;
using UnityEngine;

namespace StrainEmpire.Gameplay.Cloud
{
    /// Real cross-platform leaderboard via Firebase Realtime Database —
    /// the mobile-primary path (see docs/mobile-publishing-checklist.md).
    /// Chosen over Game Center / Google Play Games Services specifically
    /// because those two don't share data with each other; a single
    /// Firebase table serves both iOS and Android under one ranking.
    /// Requires Firebase Unity SDK (Auth + Realtime Database) imported and
    /// FIREBASE_ENABLED defined (Project Settings > Player > Scripting
    /// Define Symbols), plus google-services.json /
    /// GoogleService-Info.plist configured per platform.
    public class FirebaseLeaderboardService : ILeaderboardService
    {
        public void SubmitScore(string leaderboardName, int score)
        {
            FirebaseUser user = FirebaseAuth.DefaultInstance.CurrentUser;
            if (user == null)
            {
                Debug.LogWarning("[FirebaseLeaderboardService] No signed-in user; score not submitted.");
                return;
            }

            DatabaseReference entryRef = FirebaseDatabase.DefaultInstance
                .GetReference("leaderboards")
                .Child(leaderboardName)
                .Child(user.UserId);

            // Transaction, not a plain SetValue, so this only overwrites
            // when the new score is higher — matches SteamLeaderboardService's
            // k_ELeaderboardUploadScoreMethodKeepBest semantics.
            entryRef.RunTransaction(mutableData =>
            {
                long current = mutableData.Value is long existing ? existing : 0L;
                if (score > current)
                    mutableData.Value = (long)score;
                return TransactionResult.Success(mutableData);
            });
        }
    }
}
#endif
