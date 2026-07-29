using StrainEmpire.Gameplay;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace StrainEmpire.EditorTools
{
    /// One-click MVP scene creation. Deliberately an Editor script rather
    /// than a hand-authored .unity file: Unity itself serializes the scene
    /// when this runs, so there's no risk of shipping a malformed hand-
    /// written YAML scene that this environment has no way to verify parses
    /// (there is no Unity Editor available here — see docs/unity-project-notes.md).
    public static class SceneBootstrapper
    {
        [MenuItem("Strain Empire/Create MVP Scene")]
        public static void CreateMvpScene()
        {
            Scene scene = EditorSceneManager.NewScene(NewSceneSetup.DefaultGameObjects, NewSceneMode.Single);

            var gameManagerGo = new GameObject("GameManager");
            gameManagerGo.AddComponent<GameManager>();

            const string path = "Assets/Scenes/MVP.unity";
            EditorSceneManager.SaveScene(scene, path);
            Debug.Log($"Created MVP scene at {path}");
        }
    }
}
