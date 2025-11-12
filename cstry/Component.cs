using System;
using System.Runtime.InteropServices;

public abstract class Component
{
    public int EntityId { get; internal set; }
    protected IntPtr nativePtr;

    protected Component() { }

    // internal factory used by Script->GetComponent
    internal void Bind(int entityId, IntPtr ptr)
    {
        EntityId = entityId;
        nativePtr = ptr;
    } 
}
