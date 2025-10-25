#include <PCH/pch.h>
#include "Utils.h"
#include "Engine.h"
// #include <direct.h>   // for _getcwd

namespace Dog
{
    void PrintAndExit(const std::string& message) 
    {
        std::cerr << message << "\nPress 'Enter' to exit.\n";
        std::cin.get();
        exit(EXIT_FAILURE);
    }

    std::string GetProjectDirectory(int argc, char* argv[])
    {
        for (int i = 1; i < argc - 1; ++i) 
        {
            if (std::string(argv[i]) == "-projectdir")
            {
                return argv[i + 1]; // Return the next argument as the project directory
            }
        }
        return ""; // Return empty string if not found
    }

    void ValidateStartingDirectory(int argc, char* argv[], bool* isDevBuild)
    {
        if (argc == 1) 
        {
            PrintAndExit("No project directory provided. Use the launcher to run the engine.");
        }

        std::string projectDir = GetProjectDirectory(argc, argv);
        if (projectDir.empty())
        {
            PrintAndExit("Invalid or missing '-projectdir' argument.");
        }

        std::cout << "Project Directory: " << projectDir << '\n';

        if (projectDir != "Dev")
        {
            //FreeConsole();
            if (!SetCurrentDirectoryA(projectDir.c_str())) {
                PrintAndExit("Failed to set project directory.");
            }
        }

        if (isDevBuild) *isDevBuild = (projectDir == "Dev");

        // char cwd[_MAX_PATH];
        // if (_getcwd(cwd, _MAX_PATH))
        // {
        //     std::cout << "Current working directory is: " << cwd << '\n';
        // }
        // else 
        // {
        //     PrintAndExit("Failed to get current working directory.");
        // }
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
}
