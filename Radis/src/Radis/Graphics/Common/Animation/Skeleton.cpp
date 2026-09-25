/*****************************************************************//**
 * \file   Skeleton.cpp
 * \brief  A cooked model's skeleton, and turning its poses into skinning matrices.
 *********************************************************************/

#include <PCH/pch.h>
#include "Skeleton.h"

namespace Radis
{
    void LocalToModel(const Skeleton& skeleton, std::span<const JointPose> local, std::span<glm::mat4> model)
    {
        for (size_t i = 0; i < skeleton.JointCount(); ++i)
        {
            const JointPose& pose = local[i];
            glm::mat4        m = glm::mat4_cast(pose.rotation);
            m[0] *= pose.scale.x;
            m[1] *= pose.scale.y;
            m[2] *= pose.scale.z;
            m[3] = glm::vec4(pose.translation, 1.0f);

            const int16_t parent = skeleton.parents[i];
            model[i] = parent < 0 ? m : model[size_t(parent)] * m;
        }
    }

    void BuildSkinPalette(const Skeleton& skeleton, std::span<const glm::mat4> model, const glm::vec3& offset, std::span<SkinMatrix> palette)
    {
        for (size_t p = 0; p < skeleton.PaletteSize(); ++p)
        {
            glm::mat4 m = model[skeleton.paletteJoints[p]] * skeleton.inverseBinds[p];
            m[3] += glm::vec4(offset, 0.0f);

            const glm::mat4 rows = glm::transpose(m);
            palette[p] = { { rows[0], rows[1], rows[2] } };
        }
    }
}
