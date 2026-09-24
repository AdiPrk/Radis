#include <pch.h>
#include "ModelCook.h"
#include "Materials.h"
#include "MeshProcessing.h"
#include "ModelImport.h"
#include "ModelWriter.h"

std::filesystem::path ModelOutputPath(const std::filesystem::path& outputDir, AssetId id)
{
    return outputDir / "models" / std::format("{:016x}.dm", id);
}

ModelCookResult CookModel(const std::filesystem::path& source, const std::string& key, ModelCookContext& ctx)
{
    const auto    start = std::chrono::steady_clock::now();
    const AssetId id = MakeAssetId(key);

    ModelCookResult result{ .output = ModelOutputPath(ctx.outputDir, id) };
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
    MaterialContext                  materialContext{ *scene, key, ctx.root, ctx.textures, result.warnings, ctx.usedImages };
    std::vector<ModelFile::Material> materials;
    for (const uint32_t index : geometry.materials)
    {
        const ImportedMaterial& imported = index < scene->materials.size() ? scene->materials[index] : kDefaultMaterial;
        auto material = BuildMaterial(imported, materialContext);
        if (!material)
        {
            return fail(material.error());
        }
        materials.push_back(*material);
    }

    if (auto written = WriteModel(result.output, id, geometry, materials); !written)
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