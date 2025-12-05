#include <PCH/pch.h>
#include "MemoryWindow.h"
#include "Graphics/Vulkan/Core/Allocator.h"
#include "Engine.h"

namespace Radis::EditorWindows
{
    using json = nlohmann::json;

    // ------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------

    namespace
    {
        struct SmallEntry
        {
            std::string name;
            uint64_t size;
            std::string type;
        };

        struct MemoryCache
        {
            std::vector<std::string> lines;
            std::string lastUpdateTime = "never";
            std::string lastError;
            bool hasData = false;
            uint64_t totalBytes = 0;
        };

        // Human readable size
        std::string HumanSize(uint64_t bytes)
        {
            static constexpr const char* kSuffix[] = { "B", "KB", "MB", "GB", "TB" };

            double v = static_cast<double>(bytes);
            int i = 0;
            while (v >= 1024.0 && i < 4) {
                v /= 1024.0;
                ++i;
            }

            char buf[64];
            if (v >= 10.0)
                std::snprintf(buf, sizeof(buf), "%.0f %s", v, kSuffix[i]);
            else
                std::snprintf(buf, sizeof(buf), "%.2f %s", v, kSuffix[i]);

            return buf;
        }

        uint64_t GetU64(const json& v)
        {
            if (v.is_number_unsigned())
                return v.get<uint64_t>();
            if (v.is_number_integer())
                return static_cast<uint64_t>(v.get<int64_t>());
            if (v.is_string()) {
                try { return std::stoull(v.get<std::string>()); }
                catch (...) {}
            }
            return 0;
        }

        std::string GetStr(const json& v)
        {
            return v.is_string() ? v.get<std::string>() : std::string();
        }

        // Recursively traverse VMA JSON and gather allocation entries
        void WalkJson(const json& node,
            std::vector<std::string>& path,
            std::vector<SmallEntry>& out)
        {
            if (node.is_object())
            {
                for (const auto& [key, value] : node.items())
                {
                    path.push_back(key);

                    if (key == "Suballocations" && value.is_array())
                    {
                        std::string poolLabel;
                        if (!path.empty())
                        {
                            poolLabel = path.front();
                            for (size_t i = 1; i < path.size(); ++i)
                                poolLabel += " / " + path[i];
                        }

                        for (const auto& sub : value)
                        {
                            if (!sub.is_object()) continue;

                            std::string type = GetStr(sub.value("Type", ""));
                            if (type == "FREE") continue;

                            uint64_t size = GetU64(sub.value("Size", 0));
                            if (size == 0) continue;

                            SmallEntry e;
                            e.size = size;
                            e.type = type + (poolLabel.empty() ? "" : " (" + poolLabel + ")");

                            if (sub.contains("Name") && sub["Name"].is_string())
                            {
                                e.name = sub["Name"].get<std::string>();
                            }
                            else
                            {
                                uint64_t offset = GetU64(sub.value("Offset", 0));
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%s @ 0x%llx",
                                    type.c_str(), (unsigned long long)offset);
                                e.name = buf;
                            }

                            out.push_back(std::move(e));
                        }
                    }
                    else if (key == "DedicatedAllocations" && value.is_array())
                    {
                        for (const auto& d : value)
                        {
                            if (!d.is_object()) continue;

                            uint64_t size = GetU64(d.value("Size", 0));
                            if (size == 0) continue;

                            SmallEntry e;
                            e.size = size;
                            e.type = GetStr(d.value("Type", ""));

                            if (d.contains("Name") && d["Name"].is_string())
                                e.name = d["Name"].get<std::string>();
                            else
                                e.name = e.type.empty() ? "Dedicated" : "Dedicated " + e.type;

                            out.push_back(std::move(e));
                        }
                    }
                    else
                    {
                        WalkJson(value, path, out);
                    }

                    path.pop_back();
                }
            }
            else if (node.is_array())
            {
                for (const auto& el : node)
                    WalkJson(el, path, out);
            }
        }

        // Convert VMA json into vector<SmallEntry>
        bool CollectEntries(const json& j, std::vector<SmallEntry>& out, std::string& err)
        {
            std::vector<std::string> path;
            WalkJson(j, path, out);

            if (out.empty())
            {
                err = "No allocations found in JSON.";
                return false;
            }

            std::sort(out.begin(), out.end(),
                [](const SmallEntry& a, const SmallEntry& b) {
                    return a.size > b.size;
                });

            return true;
        }

        // Build final string lines
        void BuildLines(const std::vector<SmallEntry>& entries,
            std::vector<std::string>& lines,
            uint64_t& totalBytes)
        {
            lines.clear();
            lines.reserve(entries.size());

            totalBytes = 0;

            for (const auto& e : entries)
            {
                totalBytes += e.size;

                std::string line;
                line.reserve(e.name.size() + 64);
                line += e.name;
                line += " - ";
                line += HumanSize(e.size);
                // line += " (";
                // line += std::to_string((unsigned long long)e.size);
                // line += " B) - ";
                line += " - ";
                line += e.type;

                lines.push_back(std::move(line));
            }
        }

        // Format timestamp
        std::string CurrentTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);

            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &tt);
#else
            localtime_r(&tt, &tm);
#endif

            char buf[64];
            std::strftime(buf, sizeof(buf), "%F %T", &tm);
            return buf;
        }

        // Update cache from VMA
        bool UpdateData(MemoryCache& cache)
        {
            VmaAllocator allocator = Allocator::GetAllocator();
            if (!allocator)
            {
                cache.lastError = "Allocator is null.";
                return false;
            }

            char* statsString = nullptr;
            vmaBuildStatsString(allocator, &statsString, /*detailedMap=*/1);
            if (!statsString)
            {
                cache.lastError = "vmaBuildStatsString returned null.";
                return false;
            }

            struct FreeGuard {
                VmaAllocator a; char* s;
                ~FreeGuard() { if (s) vmaFreeStatsString(a, s); }
            } guard{ allocator, statsString };

            json j;
            try {
                j = json::parse(statsString);
            }
            catch (const std::exception& e)
            {
                cache.lastError = std::string("JSON parse error: ") + e.what();
                return false;
            }

            std::vector<SmallEntry> entries;
            if (!CollectEntries(j, entries, cache.lastError))
                return false;

            BuildLines(entries, cache.lines, cache.totalBytes);
            cache.lastUpdateTime = CurrentTimestamp();
            cache.lastError.clear();
            cache.hasData = true;
            return true;
        }

    } // namespace


    // ------------------------------------------------------------
    // Public Rendering Function
    // ------------------------------------------------------------
    void RenderMemoryWindow()
    {
        PROFILE_SCOPE("VMA Memory");

        static MemoryCache cache;

        ImGui::Begin("VMA Memory");

        if (Engine::GetGraphicsAPI() != GraphicsAPI::Vulkan)
        {
            ImGui::TextWrapped("VMA Memory window is only available when using Vulkan graphics API.");
            ImGui::End();
            return;
        }

        if (ImGui::Button("Update Now"))
            UpdateData(cache);

        ImGui::SameLine();
        ImGui::TextUnformatted(("Last: " + cache.lastUpdateTime).c_str());

        ImGui::Separator();

        if (!cache.hasData)
        {
            ImGui::TextWrapped("No cached data yet. Click \"Update Now\" to gather allocations.");
            if (!cache.lastError.empty())
                ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "%s", cache.lastError.c_str());
            ImGui::End();
            return;
        }

        ImGui::Text("Total VMA Memory: %s (%llu bytes)",
            HumanSize(cache.totalBytes).c_str(),
            (unsigned long long)cache.totalBytes);
        ImGui::Separator();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(cache.lines.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                ImGui::TextUnformatted(cache.lines[i].c_str());
        }

        if (!cache.lastError.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
                "Last update error: %s", cache.lastError.c_str());
        }

        ImGui::End();
    }

} // namespace Radis::EditorWindows
