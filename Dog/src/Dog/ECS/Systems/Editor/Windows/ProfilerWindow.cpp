#include <PCH/pch.h>
#include "ProfilerWindow.h"
#include "Profiler/Profiler.h"

namespace Dog
{
    namespace EditorWindows
    {
#if PROFILING_ENABLED

        // Small helpers
        static inline double NsToMs(uint64_t ns) { return double(ns) * 1e-6; }
        static inline float Clamp01(float v) { return (v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)); }

        // Renders the top aggregates table
        static void RenderAggregatesTable(const ProfilerSnapshot& snap, int maxRows, float minPercentFilter)
        {
            // Build index list of aggregates with call count > 0
            struct Row { size_t id; uint64_t totalNs; uint32_t calls; double avgMs; uint64_t maxNs; };
            std::vector<Row> rows;
            rows.reserve(snap.aggs.size());
            for (size_t i = 0; i < snap.aggs.size(); ++i) {
                const auto& a = snap.aggs[i];
                if (a.callCount == 0) continue;
                double pct = snap.frameTotalNs > 0 ? (100.0 * double(a.totalNs) / double(snap.frameTotalNs)) : 0.0;
                if (pct < minPercentFilter) continue;
                Row r;
                r.id = i;
                r.totalNs = a.totalNs;
                r.calls = a.callCount;
                r.avgMs = a.callCount ? NsToMs(a.totalNs / a.callCount) : 0.0;
                r.maxNs = a.maxNs;
                rows.push_back(r);
            }

            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.totalNs > b.totalNs; });

            ImGui::BeginChild("agg_table_child", ImVec2(0, 220), true);
            ImGui::Columns(5, "agg_cols", true);
            ImGui::TextUnformatted("Name"); ImGui::NextColumn();
            ImGui::TextUnformatted("Total"); ImGui::NextColumn();
            ImGui::TextUnformatted("Calls"); ImGui::NextColumn();
            ImGui::TextUnformatted("Avg"); ImGui::NextColumn();
            ImGui::TextUnformatted("Max"); ImGui::NextColumn();
            ImGui::Separator();

            int printed = 0;
            for (const auto& r : rows) {
                if (printed++ >= maxRows) break;
                const std::string& n = snap.names[r.id];
                ImGui::TextUnformatted(n.c_str()); ImGui::NextColumn();

                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.3f ms", NsToMs(r.totalNs));
                ImGui::TextUnformatted(buf); ImGui::NextColumn();

                ImGui::Text("%u", r.calls); ImGui::NextColumn();

                char buf2[64];
                std::snprintf(buf2, sizeof(buf2), "%.3f ms", r.avgMs);
                ImGui::TextUnformatted(buf2); ImGui::NextColumn();

                char buf3[64];
                std::snprintf(buf3, sizeof(buf3), "%.3f ms", NsToMs(r.maxNs));
                ImGui::TextUnformatted(buf3); ImGui::NextColumn();
            }

            ImGui::EndChild();
            ImGui::Columns(1);
        }

        // Main ImGui update function
        void RenderProfilerWindow()
        {
            // Try to get last fully-complete frame snapshot.
            ProfilerSnapshot snap;
            bool have = Profiler::GetLastFrameSnapshot(snap);
            if (!have) {
                // No snapshot yet: render placeholder with a short help text.
                if (ImGui::Begin("Profiler", nullptr)) {
                    ImGui::TextDisabled("No profiler snapshot available yet.");
                    ImGui::TextWrapped("Profiler takes a snapshot at the end of each frame. Run one frame or enable report-on-end-frame to populate.");
                }
                ImGui::End();
                return;
            }

            // UI state (could be stored in static or persistent editor state)
            static bool freeze = false;
            static bool autoRefresh = true;
            static float minPercent = 0.05f; // 0.05% minimum display
            static int maxAggRows = 25;
            static char searchBuf[128] = "";

            // If frozen, don't refresh the local 'snap' after first frame
            static ProfilerSnapshot frozenSnap;
            if (freeze) {
                if (frozenSnap.nodes.empty()) frozenSnap = snap;
            }
            else {
                frozenSnap = ProfilerSnapshot(); // clear
            }

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
                if (ImGui::Button(freeze ? "Unfreeze" : "Freeze")) {
                    freeze = !freeze;
                    if (!freeze) frozenSnap = ProfilerSnapshot(); // clear frozen copy
                }
                ImGui::SameLine();
                ImGui::Checkbox("Auto", &autoRefresh);
                ImGui::SameLine();
                ImGui::TextDisabled("  ");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy CSV")) {
                    // quick CSV: name,total_ms,calls,avg_ms,max_ms
                    std::ostringstream ss;
                    ss << "Name,TotalMs,Calls,AvgMs,MaxMs\n";
                    for (size_t i = 0; i < displaySnap.aggs.size(); ++i) {
                        const auto& a = displaySnap.aggs[i];
                        if (a.callCount == 0) continue;
                        ss << "\"" << displaySnap.names[i] << "\","
                            << NsToMs(a.totalNs) << "," << a.callCount << ","
                            << (a.callCount ? NsToMs(a.totalNs / a.callCount) : 0.0) << "," << NsToMs(a.maxNs) << "\n";
                    }
                    ImGui::SetClipboardText(ss.str().c_str());
                }
            }
            ImGui::Columns(1);
            ImGui::Separator();

            // Filters row
            ImGui::PushItemWidth(200);
            ImGui::InputTextWithHint("##search", "Filter by name...", searchBuf, sizeof(searchBuf));
            ImGui::SameLine();
            ImGui::SliderFloat("Min %", &minPercent, 0.0f, 5.0f, "%.2f%%");
            // convert percent to fraction
            float minPctFilter = minPercent * 0.01f;
            ImGui::SameLine();
            ImGui::TextDisabled("(Use Freeze to keep last shown frame)");

            ImGui::Spacing();

            // Right: Aggregates
            ImGui::BeginChild("AggregatesChild", ImVec2(0, 0), true);
            ImGui::Text("Top aggregates");
            ImGui::Separator();

            // Optionally filter by search:
            // Simple search lower-case match
            std::string search = searchBuf;
            std::transform(search.begin(), search.end(), search.begin(), ::tolower);

            // Build a temporary snapshot filtered by name if search specified.
            ProfilerSnapshot filteredSnap;
            if (!search.empty()) {
                // We'll copy names and aggs but only keep matching ones
                filteredSnap = displaySnap; // shallow copy; we'll prune after
                filteredSnap.nodes.clear(); // do not need nodes here for aggregates view
                for (size_t i = 0; i < displaySnap.aggs.size(); ++i) {
                    if (displaySnap.aggs[i].callCount == 0) continue;
                    std::string nameLower = displaySnap.names[i];
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(search) != std::string::npos) {
                        // keep
                        // nothing to do - we will read directly from displaySnap below
                    }
                }
            }

            RenderAggregatesTable(displaySnap, maxAggRows, minPctFilter);

            ImGui::EndChild();

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
