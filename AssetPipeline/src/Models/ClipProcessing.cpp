#include <pch.h>
#include "ClipProcessing.h"
#include "Trs.h"

#include <ranges>

using AnimationFile::Channel;

static constexpr float kMaxSampleRate = 60.0f;
static constexpr float kTolerance = 1e-5f;

template <typename T, typename Mix>
static T SampleKeys(const std::vector<ImportedKey<T>>& keys, float time, Mix mix)
{
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    const auto next = std::ranges::upper_bound(keys, time, {}, &ImportedKey<T>::time);
    const auto& a = *(next - 1);
    const auto& b = *next;
    return mix(a.value, b.value, (time - a.time) / (b.time - a.time));
}

// Every node's transform at `time`, relative to its parent.
static void SampleNodes(const ImportedAnimation& animation, std::span<const Trs> stored, float time, std::span<Trs> out)
{
    std::ranges::copy(stored, out.begin());
    for (const ImportedChannel& channel : animation.channels)
    {
        Trs& node = out[channel.node];
        if (!channel.translations.empty())
            node.translation = SampleKeys(channel.translations, time, [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); });
        if (!channel.rotations.empty())
            node.rotation = glm::normalize(SampleKeys(channel.rotations, time, [](const glm::quat& a, const glm::quat& b, float t) { return glm::slerp(a, b, t); }));
        if (!channel.scales.empty())
            node.scale = SampleKeys(channel.scales, time, [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); });
    }
}

static glm::vec4 ChannelValue(const Trs& trs, Channel channel)
{
    switch (channel)
    {
    case Channel::Translation: return glm::vec4(trs.translation, 0.0f);
    case Channel::Rotation:    return glm::vec4(trs.rotation.x, trs.rotation.y, trs.rotation.z, trs.rotation.w);
    default:                   return glm::vec4(trs.scale, 0.0f);
    }
}

// Rotations q and -q are the same rotation.
static bool Near(const glm::vec4& a, const glm::vec4& b, Channel channel)
{
    const auto close = [](const glm::vec4& x, const glm::vec4& y)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (std::abs(x[i] - y[i]) > kTolerance * std::max(1.0f, std::abs(x[i])))
                    return false;
            }
            return true;
        };
    return close(a, b) || (channel == Channel::Rotation && close(a, -b));
}

uint32_t SkeletonHash(const ImportedSkeleton& skeleton)
{
    std::vector<ModelFile::Joint> joints(skeleton.joints.size());
    for (size_t i = 0; i < joints.size(); ++i)
    {
        joints[i].parent = int16_t(skeleton.joints[i].parent);
        joints[i].nameHash = ModelFile::NameHash(skeleton.joints[i].name.c_str());
    }
    return AnimationFile::SkeletonHash(joints.data(), joints.size());
}

// Where a joint's pose comes from in the file. Usually the joint's node is a child of its parent
// joint's node, so the node's own transform is the joint's. Otherwise the pose is worked out from
// the node's transform relative to the file: against the parent joint's node, or against the
// parent joint's rest pose when the file doesn't have that node.
struct JointSource
{
    int32_t node = -1;          // -1: the joint isn't in the file and keeps its rest pose
    int32_t parentNode = -1;    // the parent joint's node; -1 for a root or a parent the file doesn't have
    bool    direct = false;     // the node's own transform is the joint's
};

std::expected<ProcessedClip, std::string> ProcessClip(const ImportedAnimations& source, const ImportedAnimation& animation, const ImportedSkeleton& skeleton)
{
    const size_t jointCount = skeleton.joints.size();

    std::unordered_map<std::string, int32_t> nodeByName;
    for (size_t i = 0; i < source.nodes.size(); ++i)
    {
        nodeByName.try_emplace(source.nodes[i].name, int32_t(i));
    }

    std::vector<JointSource> sources(jointCount);
    for (size_t j = 0; j < jointCount; ++j)
    {
        const ImportedJoint& joint = skeleton.joints[j];
        if (const auto found = nodeByName.find(joint.name); found != nodeByName.end())
        {
            JointSource& s = sources[j];
            s.node = found->second;
            s.parentNode = joint.parent < 0 ? -1 : sources[size_t(joint.parent)].node;

            // A root's transform is also converted to engine space.
            const int32_t nodeParent = source.nodes[size_t(s.node)].parent;
            s.direct = joint.parent < 0 ? nodeParent < 0 && source.conversion == glm::mat4(1.0f) : s.parentNode >= 0 && nodeParent == s.parentNode;
        }
    }

    const bool moves = std::ranges::any_of(animation.channels, [&](const ImportedChannel& channel)
        {
            return std::ranges::any_of(sources, [&](const JointSource& s) { return s.node == int32_t(channel.node); });
        });
    if (!moves)
    {
        return std::unexpected("it moves none of the model's joints");
    }

    ProcessedClip out;
    out.skeletonHash = SkeletonHash(skeleton);
    out.duration = std::max(animation.duration, 0.0f);
    out.sampleRate = std::clamp(std::round(animation.sampleRate), 1.0f, kMaxSampleRate);
    out.frameCount = out.duration > 0.0f ? std::max(2u, uint32_t(std::lround(out.duration * out.sampleRate)) + 1) : 1u;

    std::vector<Trs> stored(source.nodes.size());
    std::ranges::transform(source.nodes, stored.begin(), [](const ImportedNode& node) { return Decompose(node.local); });
    std::vector<Trs> rest(jointCount);
    std::ranges::transform(skeleton.joints, rest.begin(), [](const ImportedJoint& joint) { return Decompose(joint.local); });

    const bool needGlobals = std::ranges::any_of(sources, [](const JointSource& s) { return s.node >= 0 && !s.direct; });

    // Joint poses, frame-major.
    std::vector<Trs>       poses(out.frameCount * jointCount);
    std::vector<Trs>       nodes(source.nodes.size());
    std::vector<glm::mat4> globals(needGlobals ? source.nodes.size() : 0);
    for (uint32_t f = 0; f < out.frameCount; ++f)
    {
        const float time = out.frameCount > 1 ? out.duration * float(f) / float(out.frameCount - 1) : 0.0f;
        SampleNodes(animation, stored, time, nodes);
        for (size_t n = 0; n < globals.size(); ++n)
        {
            const int32_t parent = source.nodes[n].parent;
            globals[n] = parent < 0 ? Compose(nodes[n]) : globals[size_t(parent)] * Compose(nodes[n]);
        }

        std::span<Trs> pose(poses.data() + size_t(f) * jointCount, jointCount);
        for (size_t j = 0; j < jointCount; ++j)
        {
            const JointSource& s = sources[j];
            const int32_t      parent = skeleton.joints[j].parent;
            if (s.node < 0)
                pose[j] = rest[j];
            else if (s.direct)
                pose[j] = nodes[size_t(s.node)];
            else if (s.parentNode >= 0)
                pose[j] = Decompose(glm::inverse(globals[size_t(s.parentNode)]) * globals[size_t(s.node)]);
            else if (parent >= 0)
                pose[j] = Decompose(glm::inverse(skeleton.joints[size_t(parent)].global) * source.conversion * globals[size_t(s.node)]);
            else
                pose[j] = Decompose(source.conversion * globals[size_t(s.node)]);
        }
    }

    const auto at = [&](uint32_t f, size_t j) -> Trs& { return poses[size_t(f) * jointCount + j]; };

    // Neighbouring frames in the same hemisphere, so the engine can lerp them.
    for (size_t j = 0; j < jointCount; ++j)
    {
        for (uint32_t f = 1; f < out.frameCount; ++f)
        {
            if (glm::dot(at(f, j).rotation, at(f - 1, j).rotation) < 0.0f)
            {
                at(f, j).rotation = -at(f, j).rotation;
            }
        }
    }

    // Root motion: the horizontal model-space motion of the first joint whose translation moves.
    for (size_t j = 0; j < jointCount && out.frameCount > 1; ++j)
    {
        const glm::vec4 first = ChannelValue(at(0, j), Channel::Translation);
        const bool      moving = std::ranges::any_of(std::views::iota(1u, out.frameCount),
            [&](uint32_t f) { return !Near(ChannelValue(at(f, j), Channel::Translation), first, Channel::Translation); });
        if (!moving)
            continue;

        std::vector<glm::mat4> parents(out.frameCount, glm::mat4(1.0f));
        for (uint32_t f = 0; f < out.frameCount; ++f)
        {
            for (int32_t p = skeleton.joints[j].parent; p >= 0; p = skeleton.joints[size_t(p)].parent)
            {
                parents[f] = Compose(at(f, size_t(p))) * parents[f];
            }
        }

        const auto position = [&](uint32_t f) { return glm::vec3(parents[f] * glm::vec4(at(f, j).translation, 1.0f)); };
        const glm::vec3 start = position(0);
        out.rootMotion.resize(out.frameCount);
        for (uint32_t f = 0; f < out.frameCount; ++f)
        {
            const glm::vec3 motion = (position(f) - start) * glm::vec3(1.0f, 0.0f, 1.0f);
            at(f, j).translation -= glm::inverse(glm::mat3(parents[f])) * motion;
            out.rootMotion[f] = glm::vec4(motion, 0.0f);
        }
        out.rootJoint = int32_t(j);
        break;
    }

    // A track for every joint channel that leaves the rest pose: one value if it holds still.
    uint32_t animatedCount = 0;
    for (size_t j = 0; j < jointCount; ++j)
    {
        for (const Channel channel : { Channel::Translation, Channel::Rotation, Channel::Scale })
        {
            const glm::vec4 first = ChannelValue(at(0, j), channel);
            const auto      all = [&](const glm::vec4& value)
                {
                    return std::ranges::all_of(std::views::iota(0u, out.frameCount), [&](uint32_t f) { return Near(ChannelValue(at(f, j), channel), value, channel); });
                };

            if (all(ChannelValue(rest[j], channel)))
                continue;

            AnimationFile::Track& track = out.tracks.emplace_back();
            track.joint = uint16_t(j);
            track.channel = channel;
            if (all(first))
            {
                track.animated = 0;
                track.index = uint32_t(out.constants.size());
                out.constants.push_back(first);
            }
            else
            {
                track.animated = 1;
                track.index = animatedCount++;
                for (uint32_t f = 0; f < out.frameCount; ++f)
                {
                    out.samples.push_back(ChannelValue(at(f, j), channel));
                }
            }
        }
    }
    return out;
}
