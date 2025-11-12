using System;
using System.Runtime.InteropServices;
using System.Threading;

class Program
{
    static void Main(string[] args)
    {
        // optional: call RunProgram to let native set up initial entities
        string[] pargs = new string[] { "dog", "$DevBuild$" };
        int r = EngineApi.RunProgram(pargs.Length, pargs);
        Console.WriteLine($"Dog RunProgram returned {r}");

        // Print registered components
        int rc = EngineApi.Engine_GetRegisteredComponentCount();
        Console.WriteLine($"Registered components: {rc}");
        for (int i = 0; i < rc; i++)
        {
            var namePtr = EngineApi.Engine_GetRegisteredComponentName(i);
            var name = EngineApi.PtrToString(namePtr);
            var size = EngineApi.Engine_GetRegisteredComponentSize(name);
            Console.WriteLine($"  {i}: {name} (size {size})");
            int fc = EngineApi.Engine_GetComponentFieldCount(name);
            for (int f = 0; f < fc; ++f)
            {
                var finfo = EngineApi.Engine_GetComponentFieldInfo(name, f);
                Console.WriteLine($"     field: {EngineApi.PtrToString(finfo.name)} off={finfo.offset} type={finfo.type}");
            }
        }

        // Run script host
        var host = new ScriptHost();
        host.LoadAndInstantiateScripts();

        // Main loop: update a few frames while demonstrating component access
        for (int frame = 0; frame < 6000; ++frame)
        {
            float dt = 1.0f / 60.0f;

            // Update managed scripts
            host.UpdateAll(dt);

            // As a demo, read native component memory directly from managed side
            IntPtr tptr = EngineApi.Engine_GetComponentPtr(1, "Transform");
            if (tptr != IntPtr.Zero)
            {
                float x = Marshal.PtrToStructure<float>(IntPtr.Add(tptr, 0));
                float y = Marshal.PtrToStructure<float>(IntPtr.Add(tptr, 4));
                Console.WriteLine($"[Main] Entity1 Transform = ({x:F2}, {y:F2})");
            }

            Thread.Sleep(16); // ~60fps
        }

        Console.WriteLine("Main done.");
    }
}
