using System;
using System.Runtime.InteropServices;

public class Transform : Component
{
    // Offsets are known from native metadata (we assume 0 and 4)
    // For production you should query offsets dynamically and cache them
    private readonly int offX = 0;
    private readonly int offY = 4;

    public (float X, float Y) Position
    {
        get
        {
            if (nativePtr == IntPtr.Zero) return (0f, 0f);
            float x = Marshal.PtrToStructure<float>(IntPtr.Add(nativePtr, offX));
            float y = Marshal.PtrToStructure<float>(IntPtr.Add(nativePtr, offY));
            return (x, y);
        }
        set
        {
            if (nativePtr == IntPtr.Zero) return;
            // use EngineApi write helper
            IntPtr tmp = Marshal.AllocHGlobal(sizeof(float));
            try
            {
                Marshal.StructureToPtr(value.X, tmp, false);
                EngineApi.Engine_WriteComponentData(EntityId, "Transform", offX, tmp, sizeof(float));
                Marshal.StructureToPtr(value.Y, tmp, false);
                EngineApi.Engine_WriteComponentData(EntityId, "Transform", offY, tmp, sizeof(float));
            }
            finally { Marshal.FreeHGlobal(tmp); }
        }
    }
}
