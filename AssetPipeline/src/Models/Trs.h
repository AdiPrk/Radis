#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// A transform as translation, rotation and scale, the form joints are stored in.
struct Trs
{
    glm::vec3 translation{ 0.0f };
    glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 scale{ 1.0f };
};

// A mirroring transform gets a negative X scale; shear is dropped.
inline Trs Decompose(const glm::mat4& m)
{
    Trs out;
    out.scale = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
    if (glm::determinant(glm::mat3(m)) < 0.0f)
    {
        out.scale.x = -out.scale.x;
    }

    const auto axis = [&](int i) { return out.scale[i] != 0.0f ? glm::vec3(m[i]) / out.scale[i] : glm::vec3(m[i]); };
    out.rotation = glm::normalize(glm::quat_cast(glm::mat3(axis(0), axis(1), axis(2))));
    out.translation = glm::vec3(m[3]);
    return out;
}

inline glm::mat4 Compose(const Trs& trs)
{
    glm::mat4 m = glm::mat4_cast(trs.rotation);
    m[0] *= trs.scale.x;
    m[1] *= trs.scale.y;
    m[2] *= trs.scale.z;
    m[3] = glm::vec4(trs.translation, 1.0f);
    return m;
}
