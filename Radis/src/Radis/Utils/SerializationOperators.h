/*****************************************************************//**
 * \file   SerializationOperators.h
 * \brief  JSON serialization operators for glm types.
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once
#include "json.hpp"

namespace glm
{
    // Serializer for glm::vec2
    inline void to_json(nlohmann::json& j, const glm::vec2& v) {
        j = { v.x, v.y };
    }

    inline void from_json(const nlohmann::json& j, glm::vec2& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
    }

    // Serializer for glm::vec3
    inline void to_json(nlohmann::json& j, const glm::vec3& v) {
        j = { v.x, v.y, v.z };
    }

    inline void from_json(const nlohmann::json& j, glm::vec3& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
    }

    // Serializer for glm::vec4
    inline void to_json(nlohmann::json& j, const glm::vec4& v) {
        j = { v.x, v.y, v.z, v.w };
    }

    inline void from_json(const nlohmann::json& j, glm::vec4& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
        j.at(3).get_to(v.w);
    }

    // Serializer for glm::vec4
    inline void to_json(nlohmann::json& j, const glm::quat& v) {
        j = { v.x, v.y, v.z, v.w };
    }

    inline void from_json(const nlohmann::json& j, glm::quat& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
        j.at(3).get_to(v.w);
    }

    // Serializer for glm::mat4
    inline void to_json(nlohmann::json& j, const glm::mat4& m) {
        j = nlohmann::json::array();
        for (int i = 0; i < 4; ++i) {
            nlohmann::json col = nlohmann::json::array();
            for (int j_idx = 0; j_idx < 4; ++j_idx) {
                col.push_back(m[i][j_idx]);
            }
            j.push_back(col);
        }
    }

    inline void from_json(const nlohmann::json& j, glm::mat4& m) {
        for (int i = 0; i < 4; ++i) {
            for (int j_idx = 0; j_idx < 4; ++j_idx) {
                m[i][j_idx] = j.at(i).at(j_idx).get<float>();
            }
        }
    }
}