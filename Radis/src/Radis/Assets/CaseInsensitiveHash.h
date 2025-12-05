#pragma once

namespace Dog
{
    struct LowerCaseString {
        std::string s;
        LowerCaseString(std::string v) : s(v)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        }

        bool operator==(LowerCaseString const& other) const noexcept 
        {
            return s == other.s;
        }
    };

    struct LowerCaseHash {
        size_t operator()(LowerCaseString const& x) const noexcept {
            return std::hash<std::string>()(x.s);
        }
    };

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
