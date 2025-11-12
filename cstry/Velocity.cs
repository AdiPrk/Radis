using System;
using System.Runtime.InteropServices;

public class Velocity : Component
{
    private readonly int offVx = 0;
    private readonly int offVy = 4;

    public (float X, float Y) Value
    {
        get
        {
            if (nativePtr == IntPtr.Zero) return (0f, 0f);
            float vx = Marshal.PtrToStructure<float>(IntPtr.Add(nativePtr, offVx));
            float vy = Marshal.PtrToStructure<float>(IntPtr.Add(nativePtr, offVy));
            return (vx, vy);
        }
        set
        {
            if (nativePtr == IntPtr.Zero) return;
            IntPtr tmp = Marshal.AllocHGlobal(sizeof(float));
            try
            {
                Marshal.StructureToPtr(value.X, tmp, false);
                EngineApi.Engine_WriteComponentData(EntityId, "Velocity", offVx, tmp, sizeof(float));
                Marshal.StructureToPtr(value.Y, tmp, false);
                EngineApi.Engine_WriteComponentData(EntityId, "Velocity", offVy, tmp, sizeof(float));
            }
            finally { Marshal.FreeHGlobal(tmp); }
        }
    }
}
