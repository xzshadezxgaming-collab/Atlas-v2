using UnityEngine.Events;

namespace UnityEngine.UI
{
    public class Text : MonoBehaviour
    {
        public string text;
        public Font font;
        public int fontSize;
        public Color color;
        public TextAnchor alignment;
    }

    public class Image : MonoBehaviour
    {
        public Color color;
    }

    public class Button : MonoBehaviour
    {
        public class ButtonClickedEvent : UnityEvent
        {
        }

        public ButtonClickedEvent onClick { get; } = new ButtonClickedEvent();
    }

    public class Canvas : MonoBehaviour
    {
        public RenderMode renderMode;
    }

    public enum RenderMode
    {
        ScreenSpaceOverlay,
        ScreenSpaceCamera,
        WorldSpace,
    }

    public class CanvasScaler : MonoBehaviour
    {
        public enum ScaleMode
        {
            ConstantPixelSize,
            ScaleWithScreenSize,
            ConstantPhysicalSize,
        }

        public ScaleMode uiScaleMode;
        public Vector2 referenceResolution;
    }

    public class GraphicRaycaster : MonoBehaviour
    {
    }
}
