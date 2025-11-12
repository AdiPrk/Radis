using System;
using System.Reflection;

public abstract class Script
{
    public int EntityId { get; internal set; }

    public virtual void OnInit() { }
    public virtual void OnStart() { }
    public virtual void OnUpdate(float dt) { }
    public virtual void OnDestroy() { }

    public T GetComponent<T>() where T : Component, new()
    {
        // component name is type name without "Proxy" or exact class name mapping
        var t = typeof(T);
        string compName = t.Name;
        // if class is e.g. Transform, map directly. If you use proxies use naming conv.
        // Query native to ensure component exists
        int has = EngineApi.Engine_HasComponent(EntityId, compName);
        // If not present, create it in native memory by calling Engine_GetComponentPtr (which ensures allocation)
        IntPtr ptr = EngineApi.Engine_GetComponentPtr(EntityId, compName);
        // instantiate wrapper and bind
        var wrapper = new T();
        wrapper.Bind(EntityId, ptr);
        return wrapper;
    }
}
