#pragma once

namespace Dog
{

    constexpr glm::vec3 aiVecToGlm(const aiVector3D& from) noexcept {
        return glm::vec3(from.x, from.y, from.z);
    }

    constexpr glm::quat aiQuatToGlm(const aiQuaternion& from) noexcept {
        return glm::quat(from.w, from.x, from.y, from.z);
    }

    constexpr glm::mat4 aiMatToGlm(const aiMatrix4x4& from) noexcept {
        return glm::mat4(
            from.a1, from.b1, from.c1, from.d1,
            from.a2, from.b2, from.c2, from.d2,
            from.a3, from.b3, from.c3, from.d3,
            from.a4, from.b4, from.c4, from.d4);
    }

}