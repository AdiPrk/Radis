#include <pch.h>
#include "ModelCook.h"
#include "ClipWriter.h"
#include "Materials.h"
#include "MeshProcessing.h"
#include "ModelImport.h"
#include "ModelWriter.h"
#include "../FileIO.h"
#include "../Inputs.h"
#include "../OutputLayout.h"

std::filesystem::path ModelOutputPath(const std::filesystem::path& outputDir, const std::filesystem::path& source)
{
    return outputDir / kModelFolder / source.filename().replace_extension(".dm");
}

static bool SameName(const std::filesystem::path& a, const std::filesystem::path& b)
{
    const std::u8string x = a.u8string(), y = b.u8string();
    return std::ranges::equal(x, y, [](char8_t l, char8_t r) { return std::tolower(l) == std::tolower(r); });
}

// The model files in `folder`, sorted.
static std::vector<std::filesystem::path> ModelFilesIn(const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> files;
    std::error_code                    ec;
    for (std::filesystem::directory_iterator it(folder, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
    {
        std::error_code entryEc;
        if (it->is_regular_file(entryEc) && IsModelFile(it->path()))
        {
            files.push_back(it->path());
        }
    }
    std::ranges::sort(files);
    return files;
}

std::optional<std::filesystem::path> ClipOwner(const std::filesystem::path& file)
{
    const std::filesystem::path folder = file.parent_path();
    if (SameName(file.stem(), folder.filename()))
        return std::nullopt;

    for (const std::filesystem::path& candidate : ModelFilesIn(folder))
    {
        if (SameName(candidate.stem(), folder.filename()))
            return candidate;
    }
    return std::nullopt;
}

// The clip files of a model: nothing unless the model's folder is named after it.
static std::vector<std::filesystem::path> ClipFiles(const std::filesystem::path& model)
{
    if (!SameName(model.stem(), model.parent_path().filename()))
        return {};

    std::vector<std::filesystem::path> files = ModelFilesIn(model.parent_path());
    std::erase_if(files, [&](const std::filesystem::path& file) { return SameName(file.filename(), model.filename()); });
    return files;
}

// A name that's safe as a file name on every OS.
static std::string ClipFileName(std::string_view name)
{
    std::string out;
    for (const char c : name)
    {
        out += (uint8_t(c) < 0x20 || std::string_view("<>:\"/\\|?*").contains(c)) ? '_' : c;
    }
    return out.empty() ? "Animation" : out;
}

// Cooks each animation in `animations`. A file with one animation names the clip, unless it's the
// model itself; otherwise the animations name their clips. Names already taken get a number.
static void CookClips(const std::filesystem::path& file, bool modelFile, const ImportedAnimations& animations, const ImportedSkeleton& skeleton,
    const std::filesystem::path& folder, std::set<std::string>& taken, std::vector<ClipCookResult>& results)
{
    const std::u8string stem = file.stem().u8string();
    const std::string   fileName(stem.begin(), stem.end());

    for (size_t i = 0; i < animations.animations.size(); ++i)
    {
        const ImportedAnimation& animation = animations.animations[i];
        const std::string        animationName = animation.name.empty() ? std::format("Animation{}", i) : animation.name;

        const std::string base = ClipFileName(modelFile ? animationName : animations.animations.size() == 1 ? fileName : fileName + "_" + animationName);
        const auto        lower = [](std::string text)
            {
                std::ranges::transform(text, text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                return text;
            };

        std::string name = base;
        for (int n = 2; !taken.insert(lower(name)).second; ++n)
        {
            name = std::format("{}_{}", base, n);
        }

        ClipCookResult& result = results.emplace_back();
        result.source = std::format("{} '{}'", file.filename().string(), animation.name);
        result.output = folder / std::filesystem::path(std::u8string(name.begin(), name.end())).concat(".da");

        const auto clip = ProcessClip(animations, animation, skeleton);
        if (!clip)
        {
            result.error = clip.error();
            continue;
        }
        if (auto written = WriteClip(result.output, *clip); !written)
        {
            result.error = written.error();
            continue;
        }
        result.frames = clip->frameCount;
        result.tracks = uint32_t(clip->tracks.size());
        result.rootMotion = clip->rootJoint >= 0;
    }
}

ModelCookResult CookModel(const std::filesystem::path& source, ModelCookContext& ctx)
{
    const auto start = std::chrono::steady_clock::now();

    ModelCookResult result{ .output = ModelOutputPath(ctx.outputDir, source) };
    const auto fail = [&](std::string error)
        {
            result.error = std::move(error);
            return std::move(result);
        };

    auto scene = ImportModel(source);
    if (!scene)
    {
        return fail(scene.error());
    }
    result.warnings = std::move(scene->warnings);

    // Joint parents are int16_t and skin weights name palette entries with uint16_t.
    if (scene->skeleton.joints.size() > size_t(std::numeric_limits<int16_t>::max()))
    {
        return fail(std::format("the skeleton has {} joints; at most {} are supported", scene->skeleton.joints.size(), std::numeric_limits<int16_t>::max()));
    }

    const ProcessedGeometry geometry = ProcessMeshes(*scene);
    if (geometry.submeshes.empty())
    {
        return fail("no triangles are left after processing");
    }
    if (geometry.paletteJoints.size() > size_t(std::numeric_limits<uint16_t>::max()) + 1)
    {
        return fail(std::format("the skin has {} palette entries; at most 65536 are supported", geometry.paletteJoints.size()));
    }
    if (geometry.droppedWeight >= 0.001f)
    {
        result.warnings.push_back(std::format("vertices have more than 4 influences; up to {:.1f}% of a vertex's weight was dropped", geometry.droppedWeight * 100.0f));
    }

    // Only materials a submesh uses are built, so unused ones don't cook textures.
    static const ImportedMaterial kDefaultMaterial;
    const std::string                modelKey = PathKey(source);
    const std::u8string              modelName = source.stem().u8string();
    MaterialContext                  materialContext{ *scene, modelKey, std::string_view(reinterpret_cast<const char*>(modelName.data()), modelName.size()),
                                                      ctx.textures, result.warnings, ctx.usedImages };
    std::vector<BuiltMaterial>       materials;
    for (const uint32_t index : geometry.materials)
    {
        const ImportedMaterial& imported = index < scene->materials.size() ? scene->materials[index] : kDefaultMaterial;
        auto material = BuildMaterial(imported, materialContext);
        if (!material)
        {
            return fail(material.error());
        }
        materials.push_back(std::move(*material));
    }

    if (auto written = WriteModel(result.output, geometry, materials, scene->skeleton); !written)
    {
        return fail(written.error());
    }

    const std::vector<std::filesystem::path> clipFiles = ClipFiles(source);
    if (scene->skeleton.joints.empty())
    {
        if (!scene->animations.animations.empty() || !clipFiles.empty())
        {
            result.warnings.push_back("animations were skipped: the model has no skeleton to animate");
        }
    }
    else
    {
        const std::filesystem::path clipFolder = ctx.outputDir / kAnimationFolder / source.stem();
        std::set<std::string>       taken;
        CookClips(source, true, scene->animations, scene->skeleton, clipFolder, taken, result.clips);
        for (const std::filesystem::path& file : clipFiles)
        {
            std::vector<std::string> warnings;
            auto                     animations = ImportAnimations(file, warnings);
            for (const std::string& warning : warnings)
            {
                result.warnings.push_back(std::format("{}: {}", file.filename().string(), warning));
            }
            if (!animations)
            {
                result.clips.push_back({ .source = file.filename().string(), .error = animations.error() });
                continue;
            }
            CookClips(file, false, *animations, scene->skeleton, clipFolder, taken, result.clips);
        }
    }

    result.submeshes = uint32_t(geometry.submeshes.size());
    result.materials = uint32_t(materials.size());
    result.joints = uint32_t(scene->skeleton.joints.size());
    result.vertices = uint32_t(geometry.positions.size());
    result.triangles = uint32_t(geometry.indices.size() / 3);
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}