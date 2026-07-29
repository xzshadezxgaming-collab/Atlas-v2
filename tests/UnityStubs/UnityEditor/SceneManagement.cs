using UnityEngine.SceneManagement;

namespace UnityEditor.SceneManagement
{
    public enum NewSceneSetup
    {
        EmptyScene,
        DefaultGameObjects,
    }

    public enum NewSceneMode
    {
        Single,
        Additive,
    }

    public static class EditorSceneManager
    {
        public static Scene NewScene(NewSceneSetup setup, NewSceneMode mode) => new Scene { name = "New Scene" };

        public static bool SaveScene(Scene scene, string path) => true;
    }
}
