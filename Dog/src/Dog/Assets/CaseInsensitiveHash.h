#pragma once

namespace Dog
{
    struct CaseInsensitiveHash {
        size_t operator()(const std::string& s) const 
        {
            size_t hash = 5381; // Simple, effective djb2 hash
            for (unsigned char c : s) 
            {
                hash = ((hash << 5) + hash) + ::tolower(c); // hash * 33 + char
            }
            return hash;
        }
    };

    struct CaseInsensitiveEqual {
        bool operator()(const std::string& a, const std::string& b) const 
        {
            return a.length() == b.length() &&
                   std::equal(a.begin(), a.end(), b.begin(),
                   [](char ac, char bc) {
                       return ::tolower(ac) == ::tolower(bc);
                   });
        }
    };
}
