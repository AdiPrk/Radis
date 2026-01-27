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
    std::string WStringToUTF8(const std::wstring& wstr);
    
    std::string GetArg(int argc, char* argv[], const std::string& flag);
    RadisLaunch::EngineSpec LoadConfig(int argc, char* argv[], bool* isDevBuild);

    std::vector<std::string> GetFilesWithExtensions(const std::string& directoryPath, const std::vector<std::string>& extensions);

    void LaunchVSCode(const std::filesystem::path& folderPath);
}
