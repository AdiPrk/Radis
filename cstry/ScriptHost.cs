using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

public class ScriptHost
{
    private readonly string _scriptsPath;
    private readonly EngineApi _engine;
    private FileSystemWatcher _watcher;
    private AssemblyLoadContext _loadContext;
    private List<IScript> _scripts = new();

    public ScriptHost(string scriptsPath, EngineApi engine)
    {
        _scriptsPath = scriptsPath;
        _engine = engine;
        Directory.CreateDirectory(_scriptsPath);
        SetupWatcher();
        LoadScripts();
    }

    private void SetupWatcher()
    {
        _watcher = new FileSystemWatcher(_scriptsPath, "*.cs");
        _watcher.NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size | NotifyFilters.FileName;
        _watcher.Changed += (_, __) => Reload();
        _watcher.Created += (_, __) => Reload();
        _watcher.Deleted += (_, __) => Reload();
        _watcher.Renamed += (_, __) => Reload();
        _watcher.EnableRaisingEvents = true;
    }

    private void Reload()
    {
        Console.WriteLine("[ScriptHost] Detected change — reloading scripts...");
        LoadScripts();
    }

    public void LoadScripts()
    {
        try
        {
            // Unload previous
            _loadContext?.Unload();
            _scripts.Clear();

            var files = Directory.GetFiles(_scriptsPath, "*.cs");
            if (files.Length == 0)
            {
                Console.WriteLine("[ScriptHost] No scripts found.");
                return;
            }

            var syntaxTrees = files.Select(f =>
                CSharpSyntaxTree.ParseText(File.ReadAllText(f),
                    new CSharpParseOptions(LanguageVersion.Latest), f)).ToList();

            var references = new List<MetadataReference>
            {
                MetadataReference.CreateFromFile(typeof(object).Assembly.Location),
                MetadataReference.CreateFromFile(typeof(IScript).Assembly.Location),
                MetadataReference.CreateFromFile(typeof(EngineApi).Assembly.Location),
            };

            // Include system references
            var systemRefs = AppDomain.CurrentDomain.GetAssemblies()
                .Where(a => !a.IsDynamic && !string.IsNullOrEmpty(a.Location))
                .Select(a => MetadataReference.CreateFromFile(a.Location));
            references.AddRange(systemRefs);

            var compilation = CSharpCompilation.Create(
                "ScriptAssembly_" + Guid.NewGuid().ToString("N"),
                syntaxTrees,
                references,
                new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));

            using var ms = new MemoryStream();
            var result = compilation.Emit(ms);

            if (!result.Success)
            {
                Console.WriteLine("[ScriptHost] Compilation failed:");
                foreach (var d in result.Diagnostics)
                    Console.WriteLine(d.ToString());
                return;
            }

            ms.Seek(0, SeekOrigin.Begin);
            _loadContext = new AssemblyLoadContext("ScriptContext_" + Guid.NewGuid(), true);
            var assembly = _loadContext.LoadFromStream(ms);

            var scriptTypes = assembly.GetTypes()
                .Where(t => typeof(IScript).IsAssignableFrom(t) && !t.IsInterface && !t.IsAbstract);

            foreach (var type in scriptTypes)
            {
                var script = (IScript)Activator.CreateInstance(type)!;
                script.Init(_engine);
                _scripts.Add(script);
                Console.WriteLine($"[ScriptHost] Loaded {type.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine("[ScriptHost] Exception loading scripts: " + ex);
        }
    }

    public void Update(float dt)
    {
        foreach (var script in _scripts)
            script.Update(dt);
    }
}
