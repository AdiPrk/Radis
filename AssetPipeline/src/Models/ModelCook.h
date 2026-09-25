#pragma once

#include "../Textures/TextureCookQueue.h"

// Where cooked models are written: one flat folder (see OutputLayout.h), named after their source file.
std::filesystem::path ModelOutputPath(const std::filesystem::path& outputDir, const std::filesystem::path& source);

// A model in a folder of its own name ("Kiara/Kiara.fbx") owns the other model files in that
// folder as animation clips. Returns the model a clip file belongs to, or nothing for a model.
std::optional<std::filesystem::path> ClipOwner(const std::filesystem::path& file);

struct ClipCookResult
{
    std::string           source;      // the file and animation it came from
    std::filesystem::path output;
    std::string           error;       // empty on success
    uint32_t              frames = 0;
    uint32_t              tracks = 0;
    bool                  rootMotion = false;
};

struct ModelCookContext
{
    const std::filesystem::path& outputDir;
    TextureCookQueue& textures;     // receives the textures the model's materials use
    std::unordered_set<std::string>& usedImages;   // receives the PathKey of every image file they use
};

struct ModelCookResult
{
    std::filesystem::path       output;
    std::string                 error;       // empty on success
    std::vector<std::string>    warnings;
    uint32_t                    submeshes = 0;
    uint32_t                    materials = 0;
    uint32_t                    vertices = 0;
    uint32_t                    triangles = 0;
    uint32_t                    joints = 0;
    std::vector<ClipCookResult> clips;
    double                      milliseconds = 0.0;
};

// Imports, processes and writes one model, then its animation clips: its own animations and those
// of its clip files (see ClipOwner). Its textures are only queued; they're cooked with the rest of
// the build's textures.
ModelCookResult CookModel(const std::filesystem::path& source, ModelCookContext& ctx);