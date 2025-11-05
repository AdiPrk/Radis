#pragma once

namespace Dog
{
    std::string WStringToUTF8(const std::wstring& wstr);
    
    std::string GetArg(int argc, char* argv[], const std::string& flag);
    DogLaunch::EngineSpec LoadConfig(int argc, char* argv[], bool* isDevBuild);
}
