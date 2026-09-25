#include <pch.h>
#include "ModelDump.h"
#include "AnimationFormat.h"
#include "Trs.h"
#include "../FileIO.h"

#include <meshoptimizer.h>

using namespace ModelFile;

struct LoadedModel
{
    Header                        header{};
    std::vector<Section>          sections;
    std::vector<Submesh>          submeshes;
    std::vector<Material>         materials;
    std::vector<Position>         positions;
    std::vector<VertexAttributes> attributes;
    std::vector<uint32_t>         indices;
    std::vector<char>             strings;
    std::vector<Joint>            joints;
    std::vector<uint16_t>         skinJoints;
    std::vector<Matrix3x4>        inverseBinds;
    std::vector<SkinWeights>      skin;

    // The texture file name of a material slot, or nullptr for none.
    const char* TextureName(uint32_t offset) const { return offset == kNoTexture ? nullptr : strings.data() + offset; }
};

static constexpr const char* kSectionNames[] = { "Submeshes", "Materials", "Positions", "Attributes", "Indices", "Strings",
                                                 "Skeleton", "SkinJoints", "InverseBinds", "SkinWeights" };
static constexpr size_t      kRequiredSections = 6;   // the skin sections are only in skinned models
static constexpr const char* kCodecNames[] = { "none", "meshopt vertex", "meshopt index" };
static constexpr const char* kAlphaModeNames[] = { "opaque", "mask", "blend" };
static constexpr const char* kWrapNames[] = { "repeat", "clamp", "mirror" };
static constexpr const char* kTextureNames[] = { "BaseColor", "Normal", "ORM", "Emissive", "Transmission" };
static_assert(std::size(kTextureNames) == kMaterialTextureCount);

template <size_t N, typename T>
static const char* NameOf(const char* const (&names)[N], T value)
{
    return size_t(value) < N ? names[size_t(value)] : "?";
}

// Decodes a section of T elements (for indices, T is the widened type and elementSize may be 2).
template <typename T>
static std::expected<std::vector<T>, std::string> Decode(const Section& section, std::span<const std::byte> file)
{
    const std::byte* data = file.data() + section.offset;
    const size_t     decodedSize = size_t(section.elementSize) * section.elementCount;

    std::vector<std::byte> decoded(decodedSize);
    int result = 0;
    switch (section.codec)
    {
    case Codec::None:
        if (section.size != decodedSize) return std::unexpected("stored size doesn't match its elements");
        if (decodedSize > 0)   // an empty section (a model without textures has no strings) may have no buffer
            std::memcpy(decoded.data(), data, decodedSize);
        break;
    case Codec::MeshoptVertex:
        result = meshopt_decodeVertexBuffer(decoded.data(), section.elementCount, section.elementSize,
            reinterpret_cast<const unsigned char*>(data), size_t(section.size));
        break;
    case Codec::MeshoptIndex:
        result = meshopt_decodeIndexBuffer(decoded.data(), section.elementCount, section.elementSize,
            reinterpret_cast<const unsigned char*>(data), size_t(section.size));
        break;
    default:
        return std::unexpected("unknown codec");
    }
    if (result != 0)
    {
        return std::unexpected(std::format("decoding failed ({})", result));
    }

    std::vector<T> out(section.elementCount);
    if (section.elementSize == sizeof(T))
    {
        if (decodedSize > 0)
            std::memcpy(out.data(), decoded.data(), decodedSize);
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if (section.elementSize != 2) return std::unexpected("indices must be 2 or 4 bytes");
        const auto* narrow = reinterpret_cast<const uint16_t*>(decoded.data());
        std::copy(narrow, narrow + section.elementCount, out.begin());
    }
    else
    {
        return std::unexpected(std::format("element size is {}, expected {}", section.elementSize, sizeof(T)));
    }
    return out;
}

static std::expected<LoadedModel, std::string> Load(std::span<const std::byte> file)
{
    LoadedModel model;
    if (file.size() < sizeof(Header))
        return std::unexpected("file is too small");

    std::memcpy(&model.header, file.data(), sizeof(Header));
    if (model.header.magic != kMagic)
        return std::unexpected("not a cooked model");
    if (model.header.version != kVersion)
        return std::unexpected(std::format("version {}, expected {}; re-cook it", model.header.version, kVersion));

    const size_t tableEnd = sizeof(Header) + sizeof(Section) * model.header.sectionCount;
    if (file.size() < tableEnd)
        return std::unexpected("section table is truncated");

    model.sections.resize(model.header.sectionCount);
    std::memcpy(model.sections.data(), file.data() + sizeof(Header), sizeof(Section) * model.sections.size());

    bool found[std::size(kSectionNames)] = {};
    for (const Section& section : model.sections)
    {
        if (section.offset > file.size() || section.size > file.size() - section.offset)
            return std::unexpected(std::format("{} section lies outside the file", NameOf(kSectionNames, section.type)));

        std::expected<void, std::string> decoded;
        const auto into = [&](auto& vector)
            {
                auto result = Decode<typename std::remove_reference_t<decltype(vector)>::value_type>(section, file);
                if (result) vector = std::move(*result);
                else decoded = std::unexpected(result.error());
            };

        switch (section.type)
        {
        case SectionType::Submeshes:  into(model.submeshes);  break;
        case SectionType::Materials:  into(model.materials);  break;
        case SectionType::Positions:  into(model.positions);  break;
        case SectionType::Attributes: into(model.attributes); break;
        case SectionType::Indices:    into(model.indices);    break;
        case SectionType::Strings:    into(model.strings);    break;
        case SectionType::Skeleton:     into(model.joints);       break;
        case SectionType::SkinJoints:   into(model.skinJoints);   break;
        case SectionType::InverseBinds: into(model.inverseBinds); break;
        case SectionType::SkinWeights:  into(model.skin);         break;
        default: continue;   // unknown sections are skipped, as a loader would
        }

        if (!decoded)
            return std::unexpected(std::format("{} section: {}", NameOf(kSectionNames, section.type), decoded.error()));
        found[size_t(section.type)] = true;
    }

    const bool skinned = std::ranges::any_of(std::span(found).subspan(kRequiredSections), std::identity{});
    for (size_t i = 0; i < std::size(found); ++i)
    {
        if (!found[i] && (i < kRequiredSections || skinned))
            return std::unexpected(std::format("{} section is missing", kSectionNames[i]));
    }
    if (model.positions.size() != model.attributes.size())
        return std::unexpected("positions and attributes have different vertex counts");

    // Every texture offset must start a string that ends inside the table.
    const auto validName = [&](uint32_t offset)
        {
            return offset == kNoTexture || (offset < model.strings.size() && std::find(model.strings.begin() + offset, model.strings.end(), '\0') != model.strings.end());
        };
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        if (!std::ranges::all_of(model.materials[i].textures, validName))
            return std::unexpected(std::format("material {} names a texture outside the string table", i));
    }

    for (size_t i = 0; i < model.joints.size(); ++i)
    {
        const Joint& joint = model.joints[i];
        if (joint.parent < -1 || joint.parent >= int32_t(i))
            return std::unexpected(std::format("joint {} has parent {}, which doesn't come before it", i, joint.parent));
        if (!validName(joint.name) || joint.name == kNoTexture)
            return std::unexpected(std::format("joint {} has a name outside the string table", i));
        if (joint.nameHash != NameHash(model.strings.data() + joint.name))
            return std::unexpected(std::format("joint {} has the wrong name hash", i));
    }

    if (skinned)
    {
        if (model.inverseBinds.size() != model.skinJoints.size())
            return std::unexpected("the palette has different numbers of joints and inverse binds");
        if (std::ranges::any_of(model.skinJoints, [&](uint16_t joint) { return joint >= model.joints.size(); }))
            return std::unexpected("a palette entry names a joint past the skeleton");
        if (model.skin.size() != model.positions.size())
            return std::unexpected("skin weights and positions have different vertex counts");

        for (size_t i = 0; i < model.skin.size(); ++i)
        {
            const SkinWeights& s = model.skin[i];
            const uint32_t     sum = uint32_t(s.weights[0]) + s.weights[1] + s.weights[2] + s.weights[3];
            if (sum != 0 && sum != 65535)
                return std::unexpected(std::format("vertex {}'s weights sum to {}, not 65535", i, sum));
            for (int k = 0; k < 4; ++k)
            {
                if (s.weights[k] > 0 && s.joints[k] >= model.skinJoints.size())
                    return std::unexpected(std::format("vertex {} names palette entry {}, past the palette", i, s.joints[k]));
            }
        }
    }

    for (size_t i = 0; i < model.submeshes.size(); ++i)
    {
        const Submesh& s = model.submeshes[i];
        if (s.material >= model.materials.size()
            || size_t(s.firstIndex) + s.indexCount > model.indices.size()
            || size_t(s.baseVertex) + s.vertexCount > model.positions.size())
        {
            return std::unexpected(std::format("submesh {} refers outside the model", i));
        }

        const auto indices = std::span(model.indices).subspan(s.firstIndex, s.indexCount);
        if (std::ranges::any_of(indices, [&](uint32_t index) { return index >= s.vertexCount; }))
            return std::unexpected(std::format("submesh {} has an index past its vertices", i));
    }
    return model;
}

static std::vector<Trs> RestPose(const LoadedModel& model)
{
    std::vector<Trs> pose(model.joints.size());
    for (size_t i = 0; i < pose.size(); ++i)
    {
        const Joint& j = model.joints[i];
        pose[i].translation = glm::vec3(j.translation[0], j.translation[1], j.translation[2]);
        pose[i].rotation = glm::quat(j.rotation[3], j.rotation[0], j.rotation[1], j.rotation[2]);
        pose[i].scale = glm::vec3(j.scale[0], j.scale[1], j.scale[2]);
    }
    return pose;
}

// Model-space joint transforms times inverse binds, for every palette entry.
static std::vector<glm::mat4> Palette(const LoadedModel& model, std::span<const Trs> pose)
{
    std::vector<glm::mat4> jointModel(model.joints.size());
    for (size_t i = 0; i < jointModel.size(); ++i)
    {
        const int16_t parent = model.joints[i].parent;
        jointModel[i] = parent < 0 ? Compose(pose[i]) : jointModel[size_t(parent)] * Compose(pose[i]);
    }

    std::vector<glm::mat4> palette(model.skinJoints.size());
    for (size_t p = 0; p < palette.size(); ++p)
    {
        glm::mat4 inverseBind(1.0f);
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                inverseBind[column][row] = model.inverseBinds[p].rows[row][column];
            }
        }
        palette[p] = jointModel[model.skinJoints[p]] * inverseBind;
    }
    return palette;
}

static glm::vec3 Skin(const LoadedModel& model, std::span<const glm::mat4> palette, size_t vertex)
{
    const SkinWeights& s = model.skin[vertex];
    const glm::vec4    position(model.positions[vertex].xyz[0], model.positions[vertex].xyz[1], model.positions[vertex].xyz[2], 1.0f);
    if (s.weights[0] == 0)
        return glm::vec3(position);

    glm::vec3 skinned(0.0f);
    for (int k = 0; k < 4; ++k)
    {
        skinned += glm::vec3(palette[s.joints[k]] * position) * (float(s.weights[k]) / 65535.0f);
    }
    return skinned;
}

// How far skinning with the rest pose moves a vertex from its stored position. Zero when the model
// was bound in its rest pose; some files store a different pose on their nodes.
static float RestPoseError(const LoadedModel& model)
{
    const std::vector<glm::mat4> palette = Palette(model, RestPose(model));

    float error = 0.0f;
    for (size_t i = 0; i < model.skin.size(); ++i)
    {
        const glm::vec3 position(model.positions[i].xyz[0], model.positions[i].xyz[1], model.positions[i].xyz[2]);
        error = std::max(error, glm::length(Skin(model, palette, i) - position));
    }
    return error;
}

static void Print(const std::filesystem::path& path, const LoadedModel& model)
{
    const Header& h = model.header;
    std::printf("%s: bounds (%.3f, %.3f, %.3f) to (%.3f, %.3f, %.3f)\n", path.string().c_str(),
        h.boundsMin[0], h.boundsMin[1], h.boundsMin[2], h.boundsMax[0], h.boundsMax[1], h.boundsMax[2]);

    std::printf("sections:\n");
    for (const Section& s : model.sections)
    {
        const uint64_t decoded = uint64_t(s.elementSize) * s.elementCount;
        std::printf("  %-12s %-14s %7u x %2u bytes  %9llu -> %9llu bytes\n", NameOf(kSectionNames, s.type), NameOf(kCodecNames, s.codec),
            s.elementCount, s.elementSize, static_cast<unsigned long long>(s.size), static_cast<unsigned long long>(decoded));
    }

    std::printf("submeshes:\n");
    for (size_t i = 0; i < model.submeshes.size(); ++i)
    {
        const Submesh& s = model.submeshes[i];
        std::printf("  %zu: material %u, %u triangles, %u vertices\n", i, s.material, s.indexCount / 3, s.vertexCount);
    }

    if (!model.joints.empty())
    {
        const size_t unskinned = std::ranges::count_if(model.skin, [](const SkinWeights& s) { return s.weights[0] == 0; });
        std::printf("skeleton: %zu joints, %zu palette entries, %zu of %zu vertices not skinned, rest pose moves vertices up to %.5f\n",
            model.joints.size(), model.skinJoints.size(), unskinned, model.skin.size(), RestPoseError(model));

        std::vector<uint32_t> depth(model.joints.size());
        std::vector<uint32_t> entries(model.joints.size());
        for (const uint16_t joint : model.skinJoints)
        {
            ++entries[joint];
        }
        for (size_t i = 0; i < model.joints.size(); ++i)
        {
            const Joint& j = model.joints[i];
            depth[i] = j.parent < 0 ? 0 : depth[size_t(j.parent)] + 1;
            const std::string palette = entries[i] > 0 ? std::format(" [{} palette]", entries[i]) : "";
            std::printf("  %3zu: %*s%s%s\n", i, int(depth[i] * 2), "", model.strings.data() + j.name, palette.c_str());
        }
    }

    std::printf("materials:\n");
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const Material& m = model.materials[i];
        const std::string alpha = m.alphaMode == AlphaMode::Mask ? std::format(" (cutoff {:.2f})", m.alphaCutoff) : "";
        std::printf("  %zu: %s%s%s, base color (%.2f, %.2f, %.2f, %.2f), metallic %.2f, roughness %.2f, emissive (%.2f, %.2f, %.2f) x %.2f\n",
            i, NameOf(kAlphaModeNames, m.alphaMode), alpha.c_str(), m.doubleSided ? ", double-sided" : "", m.baseColor[0], m.baseColor[1], m.baseColor[2],
            m.baseColor[3], m.metallic, m.roughness, m.emissive[0], m.emissive[1], m.emissive[2], m.emissiveStrength);

        if (m.transmission > 0.0f)
        {
            std::printf("     transmission %.2f, ior %.2f\n", m.transmission, m.ior);
        }

        for (size_t t = 0; t < kMaterialTextureCount; ++t)
        {
            if (const char* name = model.TextureName(m.textures[t]))
            {
                std::printf("     %-12s %s (%s, %s)\n", kTextureNames[t], name, NameOf(kWrapNames, m.wrapU[t]), NameOf(kWrapNames, m.wrapV[t]));
            }
        }
    }
}

struct LoadedClip
{
    AnimationFile::Header               header{};
    std::vector<AnimationFile::Section> sections;
    std::vector<AnimationFile::Track>   tracks;
    std::vector<glm::vec4>              constants;
    std::vector<glm::vec4>              samples;
    std::vector<glm::vec4>              rootMotion;
};

static constexpr const char* kClipSectionNames[] = { "Tracks", "Constants", "Samples", "RootMotion" };
static constexpr const char* kChannelNames[] = { "translation", "rotation", "scale" };

static std::expected<LoadedClip, std::string> LoadClip(std::span<const std::byte> file)
{
    LoadedClip clip;
    if (file.size() < sizeof(AnimationFile::Header))
        return std::unexpected("file is too small");

    std::memcpy(&clip.header, file.data(), sizeof(AnimationFile::Header));
    if (clip.header.version != AnimationFile::kVersion)
        return std::unexpected(std::format("version {}, expected {}; re-cook it", clip.header.version, AnimationFile::kVersion));

    const size_t tableEnd = sizeof(AnimationFile::Header) + sizeof(AnimationFile::Section) * clip.header.sectionCount;
    if (file.size() < tableEnd)
        return std::unexpected("section table is truncated");

    clip.sections.resize(clip.header.sectionCount);
    std::memcpy(clip.sections.data(), file.data() + sizeof(AnimationFile::Header), sizeof(AnimationFile::Section) * clip.sections.size());

    bool found[std::size(kClipSectionNames)] = {};
    for (const AnimationFile::Section& clipSection : clip.sections)
    {
        if (clipSection.offset > file.size() || clipSection.size > file.size() - clipSection.offset)
            return std::unexpected(std::format("{} section lies outside the file", NameOf(kClipSectionNames, clipSection.type)));

        Section section{};   // same layout, so the model decoder reads it
        std::memcpy(&section, &clipSection, sizeof(section));

        std::expected<void, std::string> decoded;
        const auto into = [&](auto& vector)
            {
                auto result = Decode<typename std::remove_reference_t<decltype(vector)>::value_type>(section, file);
                if (result) vector = std::move(*result);
                else decoded = std::unexpected(result.error());
            };

        switch (clipSection.type)
        {
        case AnimationFile::SectionType::Tracks:     into(clip.tracks);     break;
        case AnimationFile::SectionType::Constants:  into(clip.constants);  break;
        case AnimationFile::SectionType::Samples:    into(clip.samples);    break;
        case AnimationFile::SectionType::RootMotion: into(clip.rootMotion); break;
        default: continue;
        }

        if (!decoded)
            return std::unexpected(std::format("{} section: {}", NameOf(kClipSectionNames, clipSection.type), decoded.error()));
        found[size_t(clipSection.type)] = true;
    }

    const bool rootMotion = clip.header.rootJoint >= 0;
    for (size_t i = 0; i < std::size(found); ++i)
    {
        if (!found[i] && (i != size_t(AnimationFile::SectionType::RootMotion) || rootMotion))
            return std::unexpected(std::format("{} section is missing", kClipSectionNames[i]));
    }

    const uint32_t frames = clip.header.frameCount;
    if (frames == 0 || clip.samples.size() % frames != 0)
        return std::unexpected("samples don't divide into whole tracks of frameCount frames");
    if (rootMotion && clip.rootMotion.size() != frames)
        return std::unexpected("root motion doesn't have one value per frame");

    const size_t animated = clip.samples.size() / frames;
    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        const AnimationFile::Track& t = clip.tracks[i];
        if (size_t(t.channel) > size_t(AnimationFile::Channel::Scale) || t.index >= (t.animated ? animated : clip.constants.size()))
            return std::unexpected(std::format("track {} refers outside the clip", i));
    }
    return clip;
}

// A clip cooked into Models/Animations/<model>/ belongs to Models/<model>.dm.
static std::filesystem::path ClipModelPath(const std::filesystem::path& clip)
{
    const std::filesystem::path modelFolder = clip.parent_path();
    return modelFolder.parent_path().parent_path() / std::filesystem::path(modelFolder.filename()).concat(".dm");
}

static std::expected<void, std::string> CheckClipAgainstModel(const LoadedClip& clip, const LoadedModel& model)
{
    if (clip.header.skeletonHash != AnimationFile::SkeletonHash(model.joints.data(), model.joints.size()))
        return std::unexpected("it was cooked for a different skeleton");
    if (clip.header.rootJoint >= int32_t(model.joints.size()))
        return std::unexpected("its root joint is past the skeleton");
    if (std::ranges::any_of(clip.tracks, [&](const AnimationFile::Track& t) { return t.joint >= model.joints.size(); }))
        return std::unexpected("a track names a joint past the skeleton");
    return {};
}

// The pose of a frame: the rest pose with the clip's tracks applied.
static std::vector<Trs> ClipPose(const LoadedModel& model, const LoadedClip& clip, uint32_t frame)
{
    std::vector<Trs> pose = RestPose(model);
    for (const AnimationFile::Track& t : clip.tracks)
    {
        const glm::vec4 v = t.animated ? clip.samples[size_t(t.index) * clip.header.frameCount + frame] : clip.constants[t.index];
        Trs&            joint = pose[t.joint];
        switch (t.channel)
        {
        case AnimationFile::Channel::Translation: joint.translation = glm::vec3(v); break;
        case AnimationFile::Channel::Rotation:    joint.rotation = glm::quat(v.w, v.x, v.y, v.z); break;
        case AnimationFile::Channel::Scale:       joint.scale = glm::vec3(v); break;
        }
    }
    return pose;
}

// The skinned model's bounds over every frame, with root motion applied, next to its rest bounds:
// a clip sampled in the wrong space or scale shows up here.
static void PrintClipBounds(const LoadedModel& model, const LoadedClip& clip)
{
    glm::vec3 restMin(std::numeric_limits<float>::max()), restMax(std::numeric_limits<float>::lowest());
    for (const Position& p : model.positions)
    {
        restMin = glm::min(restMin, glm::vec3(p.xyz[0], p.xyz[1], p.xyz[2]));
        restMax = glm::max(restMax, glm::vec3(p.xyz[0], p.xyz[1], p.xyz[2]));
    }

    glm::vec3 clipMin(std::numeric_limits<float>::max()), clipMax(std::numeric_limits<float>::lowest());
    for (uint32_t f = 0; f < clip.header.frameCount; ++f)
    {
        const std::vector<glm::mat4> palette = Palette(model, ClipPose(model, clip, f));
        const glm::vec3              motion = clip.header.rootJoint >= 0 ? glm::vec3(clip.rootMotion[f]) : glm::vec3(0.0f);
        for (size_t i = 0; i < model.positions.size(); ++i)
        {
            const glm::vec3 skinned = Skin(model, palette, i) + motion;
            clipMin = glm::min(clipMin, skinned);
            clipMax = glm::max(clipMax, skinned);
        }
    }

    std::printf("bounds: rest (%.3f, %.3f, %.3f) to (%.3f, %.3f, %.3f), over the clip (%.3f, %.3f, %.3f) to (%.3f, %.3f, %.3f)\n",
        restMin.x, restMin.y, restMin.z, restMax.x, restMax.y, restMax.z, clipMin.x, clipMin.y, clipMin.z, clipMax.x, clipMax.y, clipMax.z);
}

static void PrintClip(const std::filesystem::path& path, const LoadedClip& clip, const LoadedModel* model)
{
    const AnimationFile::Header& h = clip.header;
    const auto jointName = [&](size_t joint) { return model ? model->strings.data() + model->joints[joint].name : "?"; };

    std::printf("%s: %u frames over %.3f s (sampled at %.0f fps), skeleton %08x\n", path.string().c_str(), h.frameCount, h.duration, h.sampleRate, h.skeletonHash);

    std::printf("sections:\n");
    for (const AnimationFile::Section& s : clip.sections)
    {
        const uint64_t decoded = uint64_t(s.elementSize) * s.elementCount;
        std::printf("  %-12s %-14s %7u x %2u bytes  %9llu -> %9llu bytes\n", NameOf(kClipSectionNames, s.type), NameOf(kCodecNames, s.codec),
            s.elementCount, s.elementSize, static_cast<unsigned long long>(s.size), static_cast<unsigned long long>(decoded));
    }

    uint32_t counts[3][2] = {};
    for (const AnimationFile::Track& t : clip.tracks)
    {
        ++counts[size_t(t.channel)][t.animated];
    }
    std::printf("tracks: %zu\n", clip.tracks.size());
    for (size_t c = 0; c < 3; ++c)
    {
        std::printf("  %-12s %3u animated, %3u constant\n", kChannelNames[c], counts[c][1], counts[c][0]);
    }

    if (h.rootJoint >= 0)
    {
        const glm::vec4 motion = clip.rootMotion.back() - clip.rootMotion.front();
        std::printf("root motion: from joint %d (%s), moves (%.3f, %.3f, %.3f) over the clip\n", h.rootJoint, jointName(size_t(h.rootJoint)), motion.x, motion.y, motion.z);
    }

    if (model)
    {
        std::printf("matches %s\n", ClipModelPath(path).string().c_str());
        PrintClipBounds(*model, clip);
    }
    else
    {
        std::printf("no model at %s to check the skeleton against\n", ClipModelPath(path).string().c_str());
    }
}

static std::expected<void, std::string> DumpClip(const std::filesystem::path& path, std::span<const std::byte> file)
{
    const auto clip = LoadClip(file);
    if (!clip)
        return std::unexpected(clip.error());

    std::optional<LoadedModel> model;
    if (const auto modelFile = ReadFile(ClipModelPath(path)))
    {
        auto loaded = Load(*modelFile);
        if (!loaded)
            return std::unexpected(std::format("{}: {}", ClipModelPath(path).string(), loaded.error()));
        if (const auto checked = CheckClipAgainstModel(*clip, *loaded); !checked)
            return std::unexpected(std::format("doesn't fit {}: {}", ClipModelPath(path).string(), checked.error()));
        model = std::move(*loaded);
    }

    PrintClip(path, *clip, model ? &*model : nullptr);
    return {};
}

std::expected<void, std::string> DumpModel(const std::filesystem::path& path)
{
    const auto file = ReadFile(path);
    if (!file)
        return std::unexpected(file.error());

    uint32_t magic = 0;
    std::memcpy(&magic, file->data(), std::min(file->size(), sizeof(magic)));
    if (magic == AnimationFile::kMagic)
        return DumpClip(path, *file);

    const auto model = Load(*file);
    if (!model)
        return std::unexpected(model.error());

    Print(path, *model);
    return {};
}