#include <pch.h>
#include "ModelCook.h"
#include "Materials.h"
#include "MeshProcessing.h"
#include "ModelImport.h"
#include "ModelWriter.h"
#include "../FileIO.h"
#include "../OutputLayout.h"

std::filesystem::path ModelOutputPath(const std::filesystem::path& outputDir, const std::filesystem::path& source)
{
    return outputDir / kModelFolder / source.filename().replace_extension(".dm");
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

    const ProcessedGeometry geometry = ProcessMeshes(*scene);
    if (geometry.submeshes.empty())
    {
        return fail("no triangles are left after processing");
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

    if (auto written = WriteModel(result.output, geometry, materials); !written)
    {
        return fail(written.error());
    }

    result.submeshes = uint32_t(geometry.submeshes.size());
    result.materials = uint32_t(materials.size());
    result.vertices = uint32_t(geometry.positions.size());
    result.triangles = uint32_t(geometry.indices.size() / 3);
    result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}