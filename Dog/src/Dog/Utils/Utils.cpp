#include <PCH/pch.h>
#include "Utils.h"
#include "Engine.h"
#include <tchar.h>
#include <shellapi.h> // For ShellExecuteA

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
    
    std::vector<std::string> GetFilesWithExtensions(const std::string& directoryPath, const std::vector<std::string>& extensions)
    {
        std::vector<std::string> fileNames;
        try
        {
            // Check if the directory exists
            if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath))
            {
                // You could log an error here if you want
                return fileNames;
            }

            // Iterate over each entry in the directory
            for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
            {
                if (entry.is_regular_file())
                {
                    // Get the file extension
                    std::string extension = entry.path().extension().string();

                    // Convert extension to lower case for case-insensitive comparison
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    // Check if the file's extension is in our list of desired extensions
                    for (const auto& desiredExt : extensions)
                    {
                        if (extension == desiredExt)
                        {
                            // Add the filename to our list
                            std::string fn = entry.path().string();
                            std::replace(fn.begin(), fn.end(), '\\', '/'); // Normalize to forward slashes
                            fileNames.push_back(fn);
                            break; // Move to the next file
                        }
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            // Handle potential exceptions, e.g., permission errors
            // For now, we can just print to stderr
            fprintf(stderr, "Filesystem error: %s\n", e.what());
        }

        return fileNames;
    }

    /**
 * @brief Executes a command and captures its standard output.
 * @param cmd The command to execute (e.g., "vswhere.exe -latest").
 * @return The stdout output from the command as a string.
 */
    std::string ExecAndGetOutput(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        // Use _popen to open a pipe to the command's stdout
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
        if (!pipe) {
            return "ERROR: _popen() failed!";
        }
        // Read the output a chunk at a time
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    /**
     * @brief Finds the latest Visual Studio installation and launches it to
     * open the specified folder.
     * @param folderPath The absolute path to the folder you want to open.
     */
    void LaunchVSForFolder(const std::string& folderPath)
    {
        // 1. Find vswhere.exe. It's in a standard location.
        const char* vswherePath = "%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe";

        // 2. Build the command to find the latest VS installation path
        //    -latest: Finds the newest installed version.
        //    -property installationPath: Asks for just the root path.
        std::string command = "\"";
        command += vswherePath;
        command += "\" -latest -property installationPath";

        // 3. Run the command and get the output
        std::string vsInstallPath = ExecAndGetOutput(command.c_str());

        // Clean up the output string (it often has a newline)
        if (!vsInstallPath.empty()) {
            vsInstallPath.erase(vsInstallPath.find_last_not_of(" \n\r\t") + 1);
        }

        if (vsInstallPath.empty() || vsInstallPath.find("ERROR") != std::string::npos) {
            std::cerr << "Could not find Visual Studio. Is it installed?" << std::endl;
            return;
        }

        // 4. Construct the path to devenv.exe
        std::string devenvPath = vsInstallPath + "\\Common7\\IDE\\devenv.exe";

        // 5. Launch Visual Studio with the folder!
        //    We use ShellExecuteA to launch the process.
        //    - "open": The verb (like right-clicking and selecting "Open").
        //    - devenvPath: The application to run.
        //    - folderPath: The argument to pass (this is what makes it open your folder).
        HINSTANCE result = ShellExecuteA(
            NULL,           // Parent window handle
            "open",         // Verb
            devenvPath.c_str(), // File to execute
            folderPath.c_str(), // Parameters (the folder to open)
            NULL,           // Working directory
            SW_SHOWDEFAULT  // Show the window
        );

        // ShellExecute returns a value <= 32 on error
        if ((uintptr_t)result <= 32) {
            std::cerr << "Failed to launch Visual Studio." << std::endl;
        }
    }
}
