/*****************************************************************//**
 * \file   Skeleton.h
 * \brief  A cooked model's skeleton, and turning its poses into skinning matrices.
 *********************************************************************/

#pragma once

namespace Radis
{
    // A joint's transform relative to its parent.
    struct JointPose
    {
        glm::vec3 translation{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };
    };

    // A skinning matrix as the shaders read it: the top three rows of an affine transform.
    struct SkinMatrix
    {
        glm::vec4 rows[3];
    };

    // Joints come after their parents (see ModelFormat.h). The skin palette is what the vertices'
    // bone IDs index: each entry follows a joint, with the inverse bind matrix that goes with it.
    struct Skeleton
    {
        std::vector<int16_t>     parents;
        std::vector<JointPose>   restPose;
        std::vector<std::string> names;
        std::vector<uint16_t>    paletteJoints;
        std::vector<glm::mat4>   inverseBinds;    // one per palette entry
        uint32_t                 hash = 0;        // AnimationFile::SkeletonHash, which clips are checked against

        size_t JointCount() const { return parents.size(); }
        size_t PaletteSize() const { return paletteJoints.size(); }
    };

    // Joint transforms relative to the model, from a pose relative to each parent.
    void LocalToModel(const Skeleton& skeleton, std::span<const JointPose> local, std::span<glm::mat4> model);

    // Each palette entry's joint transform times its inverse bind, moved by `offset`.
    void BuildSkinPalette(const Skeleton& skeleton, std::span<const glm::mat4> model, const glm::vec3& offset, std::span<SkinMatrix> palette);
}
