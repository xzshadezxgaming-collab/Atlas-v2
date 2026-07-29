using System.Collections.Generic;

namespace UnityEngine.Events
{
    public delegate void UnityAction();

    public class UnityEvent
    {
        private readonly List<UnityAction> _listeners = new List<UnityAction>();

        public void AddListener(UnityAction call) => _listeners.Add(call);

        public void Invoke()
        {
            foreach (UnityAction listener in _listeners)
                listener();
        }
    }
}
