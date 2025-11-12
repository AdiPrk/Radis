using System;
using System.Runtime.InteropServices;

public static class EngineApi
{
    const string DLL = "C:\\Code\\Cat2\\x64\\Debug\\Client\\Dog.dll";
    const CallingConvention CALLING_CONVENTION = CallingConvention.Cdecl;

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern int Engine_GetRegisteredComponentCount();

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern IntPtr Engine_GetRegisteredComponentName(int index);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern int Engine_GetRegisteredComponentSize([MarshalAs(UnmanagedType.LPStr)] string componentName);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern int Engine_GetComponentFieldCount([MarshalAs(UnmanagedType.LPStr)] string componentName);

    [StructLayout(LayoutKind.Sequential)]
    public struct FieldInfoC
    {
        public IntPtr name;
        public int offset;
        public int type;
        public int size;
    }

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern FieldInfoC Engine_GetComponentFieldInfo([MarshalAs(UnmanagedType.LPStr)] string componentName, int index);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern IntPtr Engine_GetComponentPtr(int entityId, [MarshalAs(UnmanagedType.LPStr)] string componentName);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern int Engine_HasComponent(int entityId, [MarshalAs(UnmanagedType.LPStr)] string componentName);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern void Engine_GetAllEntities([Out] int[] outIds, int maxCount, out int outCount);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern void Engine_WriteComponentData(int entityId, [MarshalAs(UnmanagedType.LPStr)] string componentName, int offset, IntPtr data, int size);

    [DllImport(DLL, CallingConvention = CALLING_CONVENTION)]
    public static extern int RunProgram(int argc, string[] argv);

    // helpers
    public static string PtrToString(IntPtr p) => Marshal.PtrToStringAnsi(p) ?? string.Empty;
}
