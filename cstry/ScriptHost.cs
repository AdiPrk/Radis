using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Scripting;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using System.Threading;

public class ScriptHost : IDisposable
{
    private readonly string _scriptsPath = "C:\\Code\\Cat2\\cstry\\Scripts";
    private AssemblyLoadContext _loadContext;
    private readonly List<Script> _instances = new();
    private FileSystemWatcher _watcher;
    private Timer _debounceTimer;
    private readonly object _reloadLock = new();
    private bool _isReloading = false;

    public ScriptHost()
    {
        if (!Directory.Exists(_scriptsPath)) Directory.CreateDirectory(_scriptsPath);
        SetupWatcher();
    }

    private void SetupWatcher()
    {
        _watcher = new FileSystemWatcher(_scriptsPath, "*.csx");
        _watcher.NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName | NotifyFilters.CreationTime;
        _watcher.Changed += OnFsChange;
        _watcher.Created += OnFsChange;
        _watcher.Renamed += OnFsChange;
        _watcher.Deleted += OnFsChange;
        _watcher.EnableRaisingEvents = true;
    }

    private void OnFsChange(object sender, FileSystemEventArgs e)
    {
        // debounce rapid sequential events (edit/save may fire multiple events)
        _debounceTimer?.Dispose();
        _debounceTimer = new Timer(_ => {
            try { LoadAndInstantiateScripts(); } catch (Exception ex) { Console.WriteLine("[ScriptHost] Reload error: " + ex); }
        }, null, 200, Timeout.Infinite);
    }

    public void LoadAndInstantiateScripts()
    {
        lock (_reloadLock)
        {
            if (_isReloading) return;
            _isReloading = true;
        }

        try
        {
            Console.WriteLine("[ScriptHost] Reloading scripts...");

            // 1) Destroy old instances
            foreach (var inst in _instances)
            {
                try { inst.OnDestroy(); }
                catch (Exception ex) { Console.WriteLine("[ScriptHost] OnDestroy threw: " + ex); }
            }
            _instances.Clear();

            // 2) Unload previous assembly context
            if (_loadContext != null)
            {
                try { _loadContext.Unload(); } catch (Exception ex) { Console.WriteLine("[ScriptHost] Unload failed: " + ex); }
                _loadContext = null;

                // attempt to aggressively collect to free the loaded assembly
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }

            // 3) compile scripts
            var files = Directory.GetFiles(_scriptsPath, "*.csx");
            if (files.Length == 0) { Console.WriteLine("[ScriptHost] No scripts found."); return; }

            var syntaxTrees = files.Select(f => CSharpSyntaxTree.ParseText(File.ReadAllText(f), path: f)).ToList();
            var refs = AppDomain.CurrentDomain.GetAssemblies()
                .Where(a => !a.IsDynamic && !string.IsNullOrEmpty(a.Location))
                .Select(a => MetadataReference.CreateFromFile(a.Location)).ToList();

            var compilation = CSharpCompilation.Create("ScriptAssembly_" + Guid.NewGuid().ToString("N"),
                syntaxTrees,
                refs,
                new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

            using var ms = new System.IO.MemoryStream();
            var result = compilation.Emit(ms);
            if (!result.Success)
            {
                Console.WriteLine("[ScriptHost] Compilation errors:");
                foreach (var d in result.Diagnostics) Console.WriteLine(d.ToString());
                return;
            }
            ms.Seek(0, System.IO.SeekOrigin.Begin);

            // 4) load assembly into new context
            _loadContext = new AssemblyLoadContext("scripts-" + Guid.NewGuid(), isCollectible: true);
            var asm = _loadContext.LoadFromStream(ms);

            // 5) find Script-derived types
            var types = asm.GetTypes().Where(t => !t.IsAbstract && typeof(Script).IsAssignableFrom(t)).ToList();
            if (types.Count == 0) { Console.WriteLine("[ScriptHost] No Script types found in scripts."); return; }

            // 6) enumerate entities from native engine (snapshot)
            int[] buf = new int[1024];
            EngineApi.Engine_GetAllEntities(buf, buf.Length, out int entityCount);
            var entities = new List<int>();
            for (int i = 0; i < entityCount; ++i) entities.Add(buf[i]);
            if (entities.Count == 0) entities.Add(1); // fallback entity

            // 7) instantiate each script type for each entity, but only once per (entity, scriptType)
            foreach (var ent in entities)
            {
                foreach (var t in types)
                {
                    try
                    {
                        // create instance
                        var inst = (Script)Activator.CreateInstance(t)!;
                        inst.EntityId = ent;
                        inst.OnInit();
                        _instances.Add(inst);
                        Console.WriteLine($"[ScriptHost] Instantiated {t.FullName} for entity {ent}");
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"[ScriptHost] Failed to instantiate {t.FullName} for entity {ent}: {ex}");
                    }
                }
            }

            Console.WriteLine("[ScriptHost] Reload complete.");
        }
        finally
        {
            lock (_reloadLock) { _isReloading = false; }
        }
    }

    public void UpdateAll(float dt)
    {
        // iterate over a snapshot to protect against modification during updates
        var snapshot = _instances.ToArray();
        foreach (var s in snapshot)
        {
            try { s.OnUpdate(dt); }
            catch (Exception ex) { Console.WriteLine($"[ScriptHost] Update error: {ex}"); }
        }
    }

    public void Dispose()
    {
        try
        {
            _watcher?.Dispose();
            foreach (var inst in _instances) { try { inst.OnDestroy(); } catch { } }
            _instances.Clear();
        }
        catch { }
    }
}
