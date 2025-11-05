#include <PCH/pch.h>
#include "Utils.h"
#include "Engine.h"
// #include <direct.h>   // for _getcwd

namespace Dog
{
    // --- Helper function to parse argv ---
    std::string GetArg(int argc, char* argv[], const std::string& flag)
    {
        for (int i = 1; i < argc; ++i) // Start at 1 to skip exe name
        {
            // Check if the current argument matches the flag
            if (std::string(argv[i]) == flag && (i + 1 < argc))
            {
                // Return the next argument (the value)
                return std::string(argv[i + 1]);
            }
        }
        return ""; // Flag not found
    }

    std::string WStringToUTF8(const std::wstring& wstr) {
        if (wstr.empty()) return {};

        // First, get the required buffer size
        int size_needed = WideCharToMultiByte(
            CP_UTF8,             // convert to UTF-8
            0,                   // no special flags
            wstr.data(),         // source string
            static_cast<int>(wstr.size()), // length of source
            nullptr,             // no output yet
            0,                   // calculate required size
            nullptr, nullptr     // default handling for invalid chars
        );

        std::string result(size_needed, 0);

        // Do the actual conversion
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wstr.data(),
            static_cast<int>(wstr.size()),
            result.data(),
            size_needed,
            nullptr, nullptr
        );

        return result;
    }

    DogLaunch::EngineSpec LoadConfig(int argc, char* argv[], bool* isDevBuild)
    {
        if (argc == 2 && std::string(argv[1]) == "$DevBuild$")
        {
            if (isDevBuild) {
                *isDevBuild = true;
            }
            std::cout << "Running in Dev mode." << std::endl;
            return {};
        }

        try {
            std::string configPath = GetArg(argc, argv, "-config");
            if (configPath.empty()) {
                std::cerr << "Error: No -config file provided." << std::endl;
                throw std::runtime_error("No -config file provided. Use the launcher.");
            }

            std::ifstream f(configPath);
            if (!f.is_open()) {
                std::cerr << "Error: Could not open config file: " << configPath << std::endl;
                throw std::runtime_error("Could not open config file.");
            }

            DogLaunch::EngineSpec args;
            try {
                nlohmann::json j = nlohmann::json::parse(f);
                f.close();
                std::filesystem::remove(configPath);

                args = j.get<DogLaunch::EngineSpec>();
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error: Failed to parse launch.json: " << e.what() << std::endl;
                throw;
            }

            // If it's not a dev build, change the working directory to the project path
            if (args.workingDirectory.empty()) {
                throw std::runtime_error("Project directory is empty in launch.json.");
            }

            if (!SetCurrentDirectoryA(args.workingDirectory.c_str())) {
                std::cerr << "Error: Failed to set project directory to: " << args.workingDirectory << std::endl;
                throw std::runtime_error("Failed to set project directory.");
            }
            std::cout << "Set working directory to: " << args.workingDirectory << '\n';
            
            return args;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to initialize: " << e.what() << std::endl;
            std::cin.get();
            return {};
        }
    }
}
