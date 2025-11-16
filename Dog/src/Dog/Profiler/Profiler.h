#pragma once

#ifndef PROFILING_ENABLED
#define PROFILING_ENABLED 1
#endif

#if PROFILING_ENABLED

namespace Dog
{
    struct ProfilerSnapshotNode {
        int32_t nameId = -1;
        int32_t parent = -1;
        int32_t firstChild = -1;
        int32_t nextSibling = -1;
        uint64_t totalNs = 0;   // inclusive
        uint64_t childNs = 0;   // children's inclusive
        uint32_t callCount = 0;
    };

    struct ProfilerSnapshotAggregate {
        uint64_t totalNs = 0;
        uint32_t callCount = 0;
        uint64_t maxNs = 0;
        uint64_t minNs = 0;
    };

    struct ProfilerSnapshot {
        std::vector<ProfilerSnapshotNode> nodes;      // node pool for last frame
        std::vector<std::string> names;               // nameId -> string
        std::vector<ProfilerSnapshotAggregate> aggs;  // per-name aggregates
        uint64_t frameTotalNs = 0;
        int32_t rootIndex = -1;
        uint64_t frameIndex = 0; // optional: frame counter
    };

    class Profiler {
    public:
        // Must be called once at app initialization (optional).
        static void Initialize(size_t reserveNodes = 4096, size_t reserveNames = 256);

        // Per-frame lifecycle
        static void BeginFrame();
        static void EndFrame();

        // CPU scope timing interface
        static void BeginScope(const char* name);
        static void EndScope();

        // Print a textual report of the last recorded frame
        static void ReportFrame();

        // Control whether a report should be emitted automatically at EndFrame
        static void SetReportOnEndFrame(bool on);

        static bool GetLastFrameSnapshot(ProfilerSnapshot& out);

    private:
        // Non-instantiable - static-only class implemented in .cpp
        Profiler() = delete;
        ~Profiler() = delete;
    };

    // Populate `out` with the last fully-recorded frame snapshot.
    // Returns true if a snapshot was available (otherwise returns false).
    static inline bool Profiler_GetLastFrameSnapshot(ProfilerSnapshot& out) {
        // wrapper to call static method on Profiler class
        return Profiler::GetLastFrameSnapshot(out);
    }

    // RAII helper that begins a scope on construction and ends it on destruction
    struct ProfilerScope {
        explicit ProfilerScope(const char* name) : m_name(name) { Profiler::BeginScope(m_name); }
        ~ProfilerScope() { Profiler::EndScope(); }
        const char* m_name;
    };

    // Convenience macros
#define PROFILE_SCOPE(name) ProfilerScope CONCAT_PROF(_profScope_, __LINE__)(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

#define CONCAT_IMPL(a,b) a##b
#define CONCAT_PROF(a,b) CONCAT_IMPL(a,b)

}

#else // PROFILING_ENABLED not defined

// No-op versions when profiling disabled
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()

namespace Profiler {
    static void Initialize() {}
    static void BeginFrame() {}
    static void EndFrame() {}
    static void BeginScope(const char*) {}
    static void EndScope(const char*) {}
    static void ReportFrame() {}
    static void SetReportOnEndFrame(bool) {}
}

#endif // PROFILING_ENABLED
