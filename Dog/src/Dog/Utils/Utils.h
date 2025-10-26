#pragma once

namespace Dog
{
    void ValidateStartingDirectory(int argc, char* argv[], bool* isDevBuild);
    std::string WStringToUTF8(const std::wstring& wstr);
}
