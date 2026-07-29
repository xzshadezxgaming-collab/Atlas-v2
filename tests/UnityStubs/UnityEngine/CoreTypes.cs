using System;
using System.Collections.Generic;

namespace UnityEngine
{
    public class Object
    {
        public string name;

        private static readonly List<Object> _all = new List<Object>();

        protected Object() => _all.Add(this);

        public static T FindObjectOfType<T>() where T : Object
        {
            foreach (Object o in _all)
                if (o is T t) return t;
            return null;
        }
    }

    public class Component : Object
    {
        public GameObject gameObject { get; internal set; }
        public Transform transform => gameObject?.transform;

        public T GetComponent<T>() where T : Component => gameObject.GetComponent<T>();

        public T GetComponentInChildren<T>() where T : Component => gameObject.GetComponentInChildren<T>();
    }

    public class MonoBehaviour : Component
    {
    }

    public class GameObject : Object
    {
        private readonly List<Component> _components = new List<Component>();
        public bool activeSelf { get; private set; } = true;

        public Transform transform { get; }

        public GameObject() : this(string.Empty)
        {
        }

        public GameObject(string name, params Type[] componentsToAdd)
        {
            this.name = name;
            transform = AddComponentInternal<Transform>();
            foreach (Type t in componentsToAdd)
                AddComponentByType(t);
        }

        public T AddComponent<T>() where T : Component, new() => AddComponentInternal<T>();

        public Component AddComponentByType(Type t)
        {
            var component = (Component)Activator.CreateInstance(t);
            component.gameObject = this;
            _components.Add(component);
            return component;
        }

        private T AddComponentInternal<T>() where T : Component, new()
        {
            var component = new T { gameObject = this };
            _components.Add(component);
            return component;
        }

        public T GetComponent<T>() where T : Component
        {
            foreach (Component c in _components)
                if (c is T t) return t;
            return null;
        }

        public T GetComponentInChildren<T>() where T : Component => GetComponent<T>();

        public void SetActive(bool value) => activeSelf = value;
    }

    public class Transform : Component
    {
        public void SetParent(Transform parent, bool worldPositionStays) { }
    }

    public class RectTransform : Transform
    {
        public Vector2 anchorMin;
        public Vector2 anchorMax;
        public Vector2 pivot;
        public Vector2 anchoredPosition;
        public Vector2 sizeDelta;
    }

    public struct Vector2
    {
        public float x, y;

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public static Vector2 zero => new Vector2(0f, 0f);
    }

    public struct Color
    {
        public float r, g, b, a;

        public Color(float r, float g, float b, float a)
        {
            this.r = r;
            this.g = g;
            this.b = b;
            this.a = a;
        }

        public static Color white => new Color(1f, 1f, 1f, 1f);
    }

    public static class Mathf
    {
        public static float Clamp01(float value) => value < 0f ? 0f : (value > 1f ? 1f : value);
        public static int RoundToInt(float f) => (int)Math.Round(f, MidpointRounding.AwayFromZero);
        public static int CeilToInt(float f) => (int)Math.Ceiling(f);
    }

    public static class Debug
    {
        public static void Log(object message) => System.Console.WriteLine(message);
        public static void LogWarning(object message) => System.Console.WriteLine(message);
        public static void LogError(object message) => System.Console.WriteLine(message);
    }

    public static class Application
    {
        public static string persistentDataPath => System.IO.Path.GetTempPath();
    }

    public static class Time
    {
        public static float deltaTime => 0f;
    }

    public static class Random
    {
        private static readonly System.Random _random = new System.Random();
        public static int Range(int minInclusive, int maxExclusive) => _random.Next(minInclusive, maxExclusive);
    }

    public static class JsonUtility
    {
        public static string ToJson(object obj, bool prettyPrint = false) =>
            System.Text.Json.JsonSerializer.Serialize(obj);

        public static T FromJson<T>(string json) =>
            System.Text.Json.JsonSerializer.Deserialize<T>(json);
    }

    public static class Resources
    {
        public static T GetBuiltinResource<T>(string path) where T : Object, new() => new T();
    }

    public class Font : Object
    {
    }

    public enum TextAnchor
    {
        UpperLeft, UpperCenter, UpperRight,
        MiddleLeft, MiddleCenter, MiddleRight,
        LowerLeft, LowerCenter, LowerRight,
    }
}
