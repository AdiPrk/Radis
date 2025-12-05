#pragma once

namespace Radis
{
    std::string WStringToUTF8(const std::wstring& wstr);
    
    std::string GetArg(int argc, char* argv[], const std::string& flag);
    RadisLaunch::EngineSpec LoadConfig(int argc, char* argv[], bool* isDevBuild);

    std::vector<std::string> GetFilesWithExtensions(const std::string& directoryPath, const std::vector<std::string>& extensions);

    void LaunchVSCode(const std::filesystem::path& folderPath);
}
