#include <PCH/pch.h>
#include "ProfilerWindow.h"
#include "Profiler/Profiler.h"

#include <algorithm>
#include <string>
#include <cctype>
#include <vector>
#include <cstdio>
#include <cmath>

namespace Radis
{
    namespace EditorWindows
    {
#if PROFILING_ENABLED

        // Small helpers
        static inline double NsToMs(uint64_t ns) { return double(ns) * 1e-6; }
        static inline float Clamp01(float v) { return (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)); }

        // Helper: lowercase a string in-place
        static inline void ToLowerInPlace(std::string& s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        }

        // Render a single line for a node: name on the left (tree node arrow), then compact stats on the same line.
        static void RenderNodeLine(const ProfilerSnapshot& snap, int nodeIdx)
        {
            const ProfilerSnapshotNode& node = snap.nodes[nodeIdx];
            const std::string& name = (node.nameId >= 0 && node.nameId < (int)snap.names.size()) ? snap.names[node.nameId] : "<unknown>";

            double totalMs = NsToMs(node.totalNs);
            double exclusiveMs = NsToMs((node.totalNs > node.childNs) ? (node.totalNs - node.childNs) : 0ULL);
            double pct = snap.frameTotalNs > 0 ? (100.0 * double(node.totalNs) / double(snap.frameTotalNs)) : 0.0;
            uint32_t calls = node.callCount;
            double avgMs = calls ? NsToMs(node.totalNs / calls) : 0.0;
            uint64_t maxNs = 0;
            if (node.nameId >= 0 && node.nameId < (int)snap.aggs.size()) {
                maxNs = snap.aggs[node.nameId].maxNs;
            }

            // The tree label is the name. We'll render the TreeNode (arrow + label) and then place stats on the same line.
            ImGui::TextUnformatted(name.c_str());
            ImGui::SameLine(300); // adjust X position where stats start; tweak as needed for preferred layout
            ImGui::TextDisabled("%.3f ms", totalMs);
            ImGui::SameLine();
            ImGui::TextDisabled("(%.2f%%)", pct);
            ImGui::SameLine();
            ImGui::TextDisabled("excl %.3f ms", exclusiveMs);
            ImGui::SameLine();
            ImGui::TextDisabled("calls %u", calls);
            ImGui::SameLine();
            ImGui::TextDisabled("avg %.3f ms", avgMs);
            ImGui::SameLine();
            ImGui::TextDisabled("max %.3f ms", NsToMs(maxNs));
        }

        // Recursively determine if a node or any descendant matches the search string (case-insensitive),
        // and whether it meets the minPercentFilter (at least one descendant or the node itself must meet min%).
        // This is used to determine whether to show a branch when a search is active.
        static bool NodeOrDescendantMatchesFilter(const ProfilerSnapshot& snap, int nodeIdx, const std::string& searchLower, float minPctFilter)
        {
            if (nodeIdx < 0 || nodeIdx >= (int)snap.nodes.size()) return false;
            const ProfilerSnapshotNode& node = snap.nodes[nodeIdx];

            // percent filter
            double pct = snap.frameTotalNs > 0 ? (double(node.totalNs) / double(snap.frameTotalNs)) : 0.0;
            bool meetsPct = pct >= double(minPctFilter);

            // search filter
            bool matchesSearch = false;
            if (!searchLower.empty()) {
                if (node.nameId >= 0 && node.nameId < (int)snap.names.size()) {
                    std::string nameLower = snap.names[node.nameId];
                    ToLowerInPlace(nameLower);
                    if (nameLower.find(searchLower) != std::string::npos) matchesSearch = true;
                }
            }
            else {
                // no search -> treat as matched
                matchesSearch = true;
            }

            if (meetsPct && matchesSearch) return true;

            // Check children
            int child = node.firstChild;
            while (child != -1) {
                if (NodeOrDescendantMatchesFilter(snap, child, searchLower, minPctFilter)) return true;
                child = snap.nodes[child].nextSibling;
            }
            return false;
        }

        // Render the hierarchy starting at a given node's children, sorting siblings by totalNs descending.
        // `showRootChildrenOnly` can be used when you want to start from root's children (root itself isn't shown as a node).
        static void RenderHierarchyRecursive(const ProfilerSnapshot& snap, int nodeIdx, const std::string& searchLower, float minPctFilter)
        {
            // Gather children of nodeIdx
            std::vector<int> children;
            if (nodeIdx >= 0 && nodeIdx < (int)snap.nodes.size()) {
                int child = snap.nodes[nodeIdx].firstChild;
                while (child != -1) {
                    children.push_back(child);
                    child = snap.nodes[child].nextSibling;
                }
            }

            // Sort children by descending totalNs
            std::sort(children.begin(), children.end(), [&snap](int a, int b) {
                uint64_t ta = (a >= 0 && a < (int)snap.nodes.size()) ? snap.nodes[a].totalNs : 0;
                uint64_t tb = (b >= 0 && b < (int)snap.nodes.size()) ? snap.nodes[b].totalNs : 0;
                return ta > tb;
                });

            for (int childIdx : children)
            {
                // If search/minPct filters are active, check whether to show this branch at all
                if (!searchLower.empty() || minPctFilter > 0.0f) {
                    if (!NodeOrDescendantMatchesFilter(snap, childIdx, searchLower, minPctFilter)) {
                        continue; // skip entire branch
                    }
                }

                const ProfilerSnapshotNode& node = snap.nodes[childIdx];
                const std::string& name = (node.nameId >= 0 && node.nameId < (int)snap.names.size()) ? snap.names[node.nameId] : "<unknown>";

                // Determine if it's a leaf (no children that pass the filter)
                bool hasChild = false;
                int ch = node.firstChild;
                while (ch != -1) {
                    // check child's percent/search quickly to avoid showing expand arrow if no visible children
                    if (searchLower.empty() && minPctFilter <= 0.0f) {
                        hasChild = true;
                        break;
                    }
                    else {
                        if (NodeOrDescendantMatchesFilter(snap, ch, searchLower, minPctFilter)) { hasChild = true; break; }
                    }
                    ch = snap.nodes[ch].nextSibling;
                }

                // Use tree node so it can be expanded/collapsed; label only contains the name (we render stats to the same line)
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (!hasChild) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                // We want a unique id per node; use the pointer-less index based id
                ImGui::PushID(childIdx);
                if (hasChild) {
                    // When node has children we call TreeNodeEx which returns true if opened
                    bool opened = ImGui::TreeNodeEx(name.c_str(), flags);
                    // Render stats on same line (we used name as label above but want stats packed right)
                    ImGui::SameLine(300);
                    {
                        double totalMs = NsToMs(node.totalNs);
                        double exclusiveMs = NsToMs((node.totalNs > node.childNs) ? (node.totalNs - node.childNs) : 0ULL);
                        double pct = snap.frameTotalNs > 0 ? (100.0 * double(node.totalNs) / double(snap.frameTotalNs)) : 0.0;
                        uint32_t calls = node.callCount;
                        double avgMs = calls ? NsToMs(node.totalNs / calls) : 0.0;
                        uint64_t maxNs = 0;
                        if (node.nameId >= 0 && node.nameId < (int)snap.aggs.size()) {
                            maxNs = snap.aggs[node.nameId].maxNs;
                        }

                        ImGui::TextDisabled("%.3f ms", totalMs);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%.2f%%)", pct);
                        ImGui::SameLine();
                        ImGui::TextDisabled("excl %.3f ms", exclusiveMs);
                        ImGui::SameLine();
                        ImGui::TextDisabled("calls %u", calls);
                        ImGui::SameLine();
                        ImGui::TextDisabled("avg %.3f ms", avgMs);
                        ImGui::SameLine();
                        ImGui::TextDisabled("max %.3f ms", NsToMs(maxNs));
                    }

                    if (opened) {
                        // Recurse into children
                        RenderHierarchyRecursive(snap, childIdx, searchLower, minPctFilter);
                        ImGui::TreePop();
                    }
                }
                else {
                    // Leaf: use selectable-looking presentation with no push/pop
                    // Because we passed NoTreePushOnOpen, we must render the label ourselves
                    ImGui::TreeNodeEx(name.c_str(), flags);
                    ImGui::SameLine(300);
                    {
                        double totalMs = NsToMs(node.totalNs);
                        double exclusiveMs = NsToMs((node.totalNs > node.childNs) ? (node.totalNs - node.childNs) : 0ULL);
                        double pct = snap.frameTotalNs > 0 ? (100.0 * double(node.totalNs) / double(snap.frameTotalNs)) : 0.0;
                        uint32_t calls = node.callCount;
                        double avgMs = calls ? NsToMs(node.totalNs / calls) : 0.0;
                        uint64_t maxNs = 0;
                        if (node.nameId >= 0 && node.nameId < (int)snap.aggs.size()) {
                            maxNs = snap.aggs[node.nameId].maxNs;
                        }

                        ImGui::TextDisabled("%.3f ms", totalMs);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%.2f%%)", pct);
                        ImGui::SameLine();
                        ImGui::TextDisabled("excl %.3f ms", exclusiveMs);
                        ImGui::SameLine();
                        ImGui::TextDisabled("calls %u", calls);
                        ImGui::SameLine();
                        ImGui::TextDisabled("avg %.3f ms", avgMs);
                        ImGui::SameLine();
                        ImGui::TextDisabled("max %.3f ms", NsToMs(maxNs));
                    }
                }
                ImGui::PopID();
            }
        }

        // High-level entry that renders the profiling hierarchy (root's children)
        static void RenderProfilerHierarchy(const ProfilerSnapshot& snap, float minPercentFilter, const std::string& searchLower)
        {
            ImGui::BeginChild("prof_hierarchy_child", ImVec2(0, 320), true);

            // Header / legend row
            ImGui::TextUnformatted("Name"); ImGui::SameLine(300);
            ImGui::TextDisabled("Total"); ImGui::SameLine();
            ImGui::TextDisabled("%"); ImGui::SameLine();
            ImGui::TextDisabled("Exclusive"); ImGui::SameLine();
            ImGui::TextDisabled("Calls"); ImGui::SameLine();
            ImGui::TextDisabled("Avg"); ImGui::SameLine();
            ImGui::TextDisabled("Max");
            ImGui::Separator();

            // If root index is invalid, early out
            int root = snap.rootIndex;
            if (root < 0 || root >= (int)snap.nodes.size()) {
                ImGui::TextDisabled("No root node in snapshot.");
                ImGui::EndChild();
                return;
            }

            // Render children of root (top-level groups) sorted by total time descending
            RenderHierarchyRecursive(snap, root, searchLower, minPercentFilter);

            ImGui::EndChild();
        }

        // Main ImGui update function
        void RenderProfilerWindow()
        {
            PROFILE_SCOPE("Profiler");

            // Try to get last fully-complete frame snapshot.
            ProfilerSnapshot snap;
            bool have = Profiler::GetLastFrameSnapshot(snap);
            if (!have) {
                if (ImGui::Begin("Profiler", nullptr)) {
                    ImGui::TextDisabled("No profiler snapshot available yet.");
                }
                ImGui::End();
                return;
            }

            // UI state
            static bool freeze = false;
            static float minPercent = 0.05f; // 0.05% minimum display
            static int maxAggRows = 25;
            static char searchBuf[128] = "";

            // If frozen, don't refresh the local 'snap' after first frame
            static ProfilerSnapshot frozenSnap;
            const ProfilerSnapshot& displaySnap = freeze ? frozenSnap : snap;

            ImGui::Begin("Profiler");

            // Header row: Frame info and controls
            ImGui::Columns(3, "prof_header");
            {
                ImGui::Text("Frame: %llu", (unsigned long long)displaySnap.frameIndex);
                ImGui::NextColumn();

                double frameMs = NsToMs(displaySnap.frameTotalNs);
                double fps = frameMs > 0.000001 ? (1000.0 / frameMs) : 0.0;
                ImGui::Text("Frame: %.3f ms (%.1f FPS)", frameMs, fps);
                ImGui::NextColumn();

                // Controls
                if (ImGui::Button(freeze ? "Unfreeze" : "Freeze"))
                {
                    freeze = !freeze;
                    if (freeze)
                    {
                        frozenSnap = snap;
                    }
                }
            }
            ImGui::Columns(1);
            ImGui::Separator();

            // Filters row
            ImGui::PushItemWidth(200);
            ImGui::InputTextWithHint("##search", "Filter by name...", searchBuf, sizeof(searchBuf));
            ImGui::SameLine();
            ImGui::SliderFloat("Min %", &minPercent, 0.0f, 5.0f, "%.2f%%");
            float minPctFilter = minPercent * 0.01f;
            ImGui::Spacing();

            // Prepare search lower-case
            std::string search = searchBuf;
            std::string searchLower;
            if (!search.empty()) {
                searchLower = search;
                ToLowerInPlace(searchLower);
            }

            // Render the hierarchy (root's children) with sorting and expand/collapse behavior.
            RenderProfilerHierarchy(displaySnap, minPctFilter, searchLower);

            ImGui::Columns(1);

            ImGui::End();
        }
#else
        void RenderProfilerWindow()
        {
            ImGui::Begin("Profiler");
            ImGui::TextDisabled("Profiler is disabled. Rebuild with PROFILING_ENABLED defined to use the profiler.");
            ImGui::End();
        }
#endif
    }
}
