#pragma once
#include <string>
#include "GraphicsAPIs.h"
#include "Common.h"

// Use a namespace to avoid collisions
namespace RadisLaunch
{

    struct EngineSpec
    {
        std::wstring name = L"Radis Engine";             // The name of the window.
        unsigned width = 1280;                         // The width of the window.
        unsigned height = 720;                         // The height of the window.
        unsigned fps = 120;			                   // The target frames per second.
        std::string serverAddress = Radis::SERVER_IP;    // The address of the server. Defaults to online VPS server.
        uint16_t serverPort = 7777;                    // The port of the server.
        Radis::GraphicsAPI graphicsAPI = Radis::GraphicsAPI::Vulkan; // The graphics API to use.

        std::string workingDirectory = "";              // The working directory of the engine.
        bool launchWithEditor = true;                   // Whether to launch the engine with the editor.
    };

    inline void to_json(nlohmann::json& j, const EngineSpec& args) {
        j = nlohmann::json{
            {"name", args.name},
            {"width", args.width},
            {"height", args.height},
            {"fps", args.fps},
            {"serverAddress", args.serverAddress},
            {"serverPort", args.serverPort},
            {"graphicsAPI", args.graphicsAPI},
            {"workingDirectory", args.workingDirectory },
            {"launchWithEditor", args.launchWithEditor }
        };
    }

    inline void from_json(const nlohmann::json& j, EngineSpec& args) {
        j.at("name").get_to(args.name);
        j.at("width").get_to(args.width);
        j.at("height").get_to(args.height);
        j.at("fps").get_to(args.fps);
        j.at("serverAddress").get_to(args.serverAddress);
        j.at("serverPort").get_to(args.serverPort);
        j.at("graphicsAPI").get_to(args.graphicsAPI);
        j.at("workingDirectory").get_to(args.workingDirectory);
        j.at("launchWithEditor").get_to(args.launchWithEditor);
    }

} // namespace RadisLaunch