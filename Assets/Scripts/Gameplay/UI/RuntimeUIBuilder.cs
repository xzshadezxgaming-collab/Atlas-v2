using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

namespace StrainEmpire.Gameplay.UI
{
    /// Minimal helpers for building a functional (not final-art) runtime UI
    /// entirely from code, so the MVP loop doesn't depend on hand-authored
    /// prefabs/scenes that can't be verified without the Unity Editor. This
    /// is programmer-art scaffolding — replace with real UI/UX during the
    /// first in-Editor art pass.
    public static class RuntimeUIBuilder
    {
        public static Canvas CreateCanvas()
        {
            var canvasGo = new GameObject("Canvas", typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
            var canvas = canvasGo.GetComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;

            var scaler = canvasGo.GetComponent<CanvasScaler>();
            scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            scaler.referenceResolution = new Vector2(1280, 720);

            if (Object.FindObjectOfType<EventSystem>() == null)
                new GameObject("EventSystem", typeof(EventSystem), typeof(StandaloneInputModule));

            return canvas;
        }

        public static Text CreateText(Transform parent, string name, string content, Vector2 anchoredPosition, Vector2 sizeDelta, int fontSize = 20)
        {
            var go = new GameObject(name, typeof(Text));
            go.transform.SetParent(parent, false);

            var rect = go.GetComponent<RectTransform>();
            rect.anchorMin = new Vector2(0f, 1f);
            rect.anchorMax = new Vector2(0f, 1f);
            rect.pivot = new Vector2(0f, 1f);
            rect.anchoredPosition = anchoredPosition;
            rect.sizeDelta = sizeDelta;

            var text = go.GetComponent<Text>();
            text.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
            text.fontSize = fontSize;
            text.color = Color.white;
            text.text = content;
            return text;
        }

        public static Button CreateButton(Transform parent, string label, Vector2 anchoredPosition, Vector2 sizeDelta, UnityEngine.Events.UnityAction onClick)
        {
            var go = new GameObject($"Button_{label}", typeof(Image), typeof(Button));
            go.transform.SetParent(parent, false);

            var rect = go.GetComponent<RectTransform>();
            rect.anchorMin = new Vector2(0f, 1f);
            rect.anchorMax = new Vector2(0f, 1f);
            rect.pivot = new Vector2(0f, 1f);
            rect.anchoredPosition = anchoredPosition;
            rect.sizeDelta = sizeDelta;

            var image = go.GetComponent<Image>();
            image.color = new Color(0.2f, 0.2f, 0.25f, 1f);

            var button = go.GetComponent<Button>();
            button.onClick.AddListener(onClick);

            Text label_ = CreateText(go.transform, "Label", label, Vector2.zero, sizeDelta, fontSize: 16);
            label_.alignment = TextAnchor.MiddleCenter;

            return button;
        }
    }
}
