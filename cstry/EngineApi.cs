using System;
using System.Runtime.InteropServices;
using System.Threading;
using Microsoft.CodeAnalysis.CSharp.Scripting;
using Microsoft.CodeAnalysis.Scripting;

public class EngineApi : IDisposable
{
    private IntPtr _handle;

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void NativeUpdateCallback(float dt, IntPtr user);

    private NativeUpdateCallback _managedCallback; // store to keep alive
    private GCHandle _thisHandle;

    public EngineApi()
    {
        _handle = Engine_Create();
    }

    public void Init(string[] args)
    {
        // simple: marshal string[] as LPArray of LPStr (works for ANSI)
        Engine_Init(_handle, args.Length, args);
    }

    public void Tick(float dt) => Engine_Tick(_handle, dt);

    public void RegisterUpdate(Action<float> callback)
    {
        _managedCallback = (dt, user) => { callback(dt); };
        // ensure delegate isn't collected
        _thisHandle = GCHandle.Alloc(_managedCallback);
        Engine_RegisterUpdateCallback(_handle, _managedCallback, IntPtr.Zero);
    }

    public int CreateEntity(string name) => Engine_CreateEntity(_handle, name);
    public void SetEntityPosition(int id, float x, float y, float z) => Engine_SetEntityPosition(_handle, id, x, y, z);

    public void Dispose()
    {
        if (_thisHandle.IsAllocated) _thisHandle.Free();
        Engine_Destroy(_handle);
    }

    // P/Invoke declarations
    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Engine_Create();

    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Engine_Destroy(IntPtr h);

    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Engine_Init(IntPtr h, int argc, [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.LPStr)] string[] argv);

    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Engine_Tick(IntPtr h, float dt);

    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Engine_RegisterUpdateCallback(IntPtr h, NativeUpdateCallback cb, IntPtr user);

    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Engine_CreateEntity(IntPtr h, [MarshalAs(UnmanagedType.LPStr)] string name);

    [DllImport("Dog.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Engine_SetEntityPosition(IntPtr h, int entityId, float x, float y, float z);
}
