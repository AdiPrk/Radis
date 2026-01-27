/*****************************************************************//**
 * \file   Utils.h
 * \brief  Utility functions for string conversion, argument parsing, file retrieval, and launching VS Code.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    // Convert wide string to UTF-8
    std::string WStringToUTF8(const std::wstring& wstr);

    // Extract command-line argument value
    std::string GetArg(int argc, char* argv[], const std::string& flag);

    // Load engine configuration from command-line or config file
    RadisLaunch::EngineSpec LoadConfig(int argc, char* argv[], bool* isDevBuild);

    // Get all files with specified extensions in a directory
    std::vector<std::string> GetFilesWithExtensions(const std::string& directoryPath,
        const std::vector<std::string>& extensions);

    // Launch Visual Studio Code at the specified folder
    void LaunchVSCode(const std::filesystem::path& folderPath);

} // namespace Radis