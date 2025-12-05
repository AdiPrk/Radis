#include <PCH/pch.h>
#include "Profiler.h"

#if PROFILING_ENABLED

namespace Dog
{

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using ns_t = std::uint64_t;

    namespace
    {
        inline ns_t NowNs() 
        {
            return static_cast<ns_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
        }

        struct Node
        {
            size_t nameId = 0;       // index into names pool
            int parent = -1;         // parent index, -1 for root
            int firstChild = -1;     // linked list of children (index)
            int nextSibling = -1;    // next sibling index
            ns_t startNs = 0;        // start timestamp
            ns_t totalNs = 0;        // inclusive time accumulated
            ns_t childNs = 0;        // total time accumulated by children
            uint32_t callCount = 0;  // how many times this exact node was entered in this frame (usually 1)
            
            void Reset()
            {
                nameId = 0;
                parent = -1;
                firstChild = -1;
                nextSibling = -1;
                startNs = 0;
                totalNs = 0;
                childNs = 0;
                callCount = 0;
            }
        };

        struct Aggregate
        {
            ns_t totalNs = 0;
            uint32_t callCount = 0;
            ns_t maxNs = 0;
            ns_t minNs = 0;

            void AddSample(ns_t sample)
            {
                totalNs += sample;
                ++callCount;
                if (sample > maxNs) maxNs = sample;
                if (minNs == 0 || sample < minNs) minNs = sample;
            }
        };

        struct ProfilerState {
            std::vector<Node> nodes;                   // pool of node objects (indexable)
            std::vector<int> stack;                    // active node stack (indices)
            std::vector<std::string> names;            // interned names; stable memory
            std::unordered_map<std::string, size_t> nameToId; // name -> id
            std::vector<Aggregate> aggregates;         // per-name aggregates for frame
            int nextNodeIndex = 0;                     // index to allocate next node
            ns_t frameStartNs = 0;
            ns_t frameTotalNs = 0;
            int rootIndex = 0;

            ProfilerSnapshot lastFrameSnapshot;
            uint64_t lastFrameIndex = 0;

            void ResetNodes() 
            {
                // If capacity is large, just reset count and reuse
                if (!nodes.empty()) 
                {
                    // reset used nodes in place for clarity
                    for (size_t i = 0; i < nodes.size(); ++i) nodes[i].Reset();
                }
                nextNodeIndex = 0;
            }
        };

        ProfilerState& S() {
            static ProfilerState s;
            return s;
        }

    } // namespace

    // -----------------------------------------------------------------------------
    // Implementation
    // -----------------------------------------------------------------------------

    void Profiler::Initialize(size_t reserveNodes, size_t reserveNames)
    {
        auto& st = S();
        st.nodes.clear();
        st.nodes.reserve(std::max<size_t>(reserveNodes, 1024));
        // create at least one node slot for root
        st.nodes.resize(std::max<size_t>(1, reserveNodes ? 1 : 1));
        st.names.reserve(std::max<size_t>(reserveNames, 64));
        st.nameToId.reserve(reserveNames);
        st.aggregates.reserve(reserveNames);
        st.ResetNodes();
        st.stack.clear();
        st.rootIndex = -1;
    }

    // Intern a name and return the stable nameId
    static size_t InternName(const char* nameRaw) 
    {
        auto& st = S();
        if (nameRaw == nullptr) nameRaw = "<null>";
        std::string key(nameRaw);
        auto it = st.nameToId.find(key);
        if (it != st.nameToId.end()) return it->second;
        size_t id = st.names.size();
        st.names.emplace_back(std::move(key));
        st.nameToId.emplace(st.names.back(), id);
        st.aggregates.emplace_back();
        return id;
    }

    // ensure there is at least one free node and return its index
    static int AllocNode()
    {
        auto& st = S();
        if (st.nextNodeIndex >= static_cast<int>(st.nodes.size())) {
            // Grow by doubling strategy for performance
            size_t newCap = std::max<size_t>(st.nodes.size() ? st.nodes.size() * 2 : 1024, 1024);
            st.nodes.resize(newCap);
        }
        int idx = st.nextNodeIndex++;
        st.nodes[idx].Reset();
        return idx;
    }

    void Profiler::BeginFrame()
    {
        auto& st = S();
        st.frameStartNs = NowNs();
        st.frameTotalNs = 0;
        // reset per-frame node pool
        // keep storage but reset counters
        st.nextNodeIndex = 0;
        st.stack.clear();
        // create root node (index 0)
        int root = AllocNode();
        st.rootIndex = root;
        st.nodes[root].nameId = InternName("FrameRoot");
        st.nodes[root].parent = -1;
        st.nodes[root].startNs = st.frameStartNs;
        st.stack.push_back(root);
        // reset aggregates for the frame
        for (auto& a : st.aggregates)
        {
            a.totalNs = 0;
            a.callCount = 0;
            a.maxNs = 0;
            a.minNs = 0;
        }
    }

    void Profiler::EndFrame() 
    {
        auto& st = S();
        ns_t endNs = NowNs();
        st.frameTotalNs = (endNs > st.frameStartNs) ? (endNs - st.frameStartNs) : 0;

        // Unwind active stack (best-effort)
        while (!st.stack.empty()) 
        {
            int idx = st.stack.back();
            st.stack.pop_back();
            ns_t elapsed = NowNs() - st.nodes[idx].startNs;
            st.nodes[idx].totalNs += elapsed;
            if (st.nodes[idx].parent != -1) st.nodes[st.nodes[idx].parent].childNs += st.nodes[idx].totalNs;
            ++st.nodes[idx].callCount;
        }

        // --- SNAPSHOT: copy the used node range and names/aggregates into lastFrameSnapshot ---
        {
            ProfilerSnapshot& snap = st.lastFrameSnapshot;
            snap.frameTotalNs = st.frameTotalNs;
            snap.frameIndex = ++st.lastFrameIndex;

            // copy names
            snap.names = st.names;

            // nodes: only copy used nodes: [0 .. nextNodeIndex)
            int usedCount = st.nextNodeIndex;
            snap.nodes.resize(usedCount);
            for (int i = 0; i < usedCount; ++i) {
                const Node& src = st.nodes[i];
                ProfilerSnapshotNode& dst = snap.nodes[i];
                dst.nameId = static_cast<int32_t>(src.nameId);
                dst.parent = src.parent;
                dst.firstChild = src.firstChild;
                dst.nextSibling = src.nextSibling;
                dst.totalNs = src.totalNs;
                dst.childNs = src.childNs;
                dst.callCount = src.callCount;
            }

            // copy aggregates
            snap.aggs.resize(st.aggregates.size());
            for (size_t i = 0; i < st.aggregates.size(); ++i)
            {
                const Aggregate& a = st.aggregates[i];
                ProfilerSnapshotAggregate& out = snap.aggs[i];
                out.totalNs = a.totalNs;
                out.callCount = a.callCount;
                out.maxNs = a.maxNs;
                out.minNs = a.minNs;
            }

            // root index: keep root that was used (may be 0)
            snap.rootIndex = st.rootIndex;
        }
    }

    void Profiler::BeginScope(const char* name) 
    {
        auto& st = S();
        if (st.rootIndex < 0)
        {
            // If BeginFrame not called, start a minimal implicit frame
            BeginFrame();
        }
        size_t nameId = InternName(name);
        int nodeIdx = AllocNode();
        Node& n = st.nodes[nodeIdx];
        n.nameId = nameId;
        n.parent = st.stack.empty() ? -1 : st.stack.back();
        n.firstChild = -1;
        n.nextSibling = -1;
        n.startNs = NowNs();
        n.totalNs = 0;
        n.childNs = 0;
        n.callCount = 0;

        // Link into parent's child list if parent exists
        if (n.parent != -1)
        {
            // insert as head
            n.nextSibling = st.nodes[n.parent].firstChild;
            st.nodes[n.parent].firstChild = nodeIdx;
        }

        st.stack.push_back(nodeIdx);
    }

    void Profiler::EndScope() 
    {
        auto& st = S();
        if (st.stack.empty()) return;

        int nodeIdx = st.stack.back();
        st.stack.pop_back();

        ns_t now = NowNs();
        Node& n = st.nodes[nodeIdx];
        ns_t elapsed = (now > n.startNs) ? (now - n.startNs) : 0;
        n.totalNs += elapsed;
        ++n.callCount;

        // accumulate child's time into parent.childNs so parent's exclusive can be computed
        if (n.parent != -1)
        {
            st.nodes[n.parent].childNs += n.totalNs;
        }

        // update per-name aggregate
        size_t nameId = n.nameId;
        if (nameId < st.aggregates.size())
        {
            st.aggregates[nameId].AddSample(n.totalNs);
        }
    }

    bool Profiler::GetLastFrameSnapshot(ProfilerSnapshot& out)
    {
        auto& st = S();
        if (st.lastFrameSnapshot.nodes.empty()) return false; // no snapshot yet
        out = st.lastFrameSnapshot;
        return true;
    }
}

#endif // PROFILING_ENABLED