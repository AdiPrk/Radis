#include <pch.h>
#include "CommandLine.h"

static constexpr std::string_view kHelp =
R"(usage: AssetPipeline <input> [options]

  <input>                    a texture or model file, or a directory (searched recursively)

options:
  -r, --role <role>          texture role: color, linear, normal, mask (required for textures)
  -p, --platform <platform>  windows-d3d12 (default), windows-vulkan, linux-vulkan, android-vulkan
      --optimize <mode>      speed, balanced (default), size
  -o, --output <dir>         output directory (default: Cooked/<platform>)
  -h, --help                 show this help
)";

struct PlatformInfo
{
    std::string_view name;
    GpuTarget        target;
};

static constexpr PlatformInfo kPlatforms[] =
{
    { "windows-d3d12",  GpuTarget::Desktop    },
    { "windows-vulkan", GpuTarget::Desktop    },
    { "linux-vulkan",   GpuTarget::Desktop    },
    { "android-vulkan", GpuTarget::MobileASTC },
};

static constexpr std::string_view kOptimizeModes[] = { "speed", "balanced", "size" };

static std::unexpected<int> UsageError(const std::string& message)
{
    std::fprintf(stderr, "error: %s\nrun 'AssetPipeline --help' for usage\n", message.c_str());
    return std::unexpected(2);
}

std::expected<Options, int> ParseCommandLine(int argc, char** argv)
{
    Options opts;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            std::fputs(kHelp.data(), stdout);
            return std::unexpected(0);
        }

        if (!arg.starts_with('-'))
        {
            if (!opts.input.empty())
            {
                return UsageError("only one input allowed");
            }

            opts.input = arg;
            continue;
        }

        // Every option takes a value: "--name value" or "--name=value".
        std::string_view name = arg;
        std::string_view value;
        if (const size_t eq = arg.find('='); eq != std::string_view::npos)
        {
            name = arg.substr(0, eq);
            value = arg.substr(eq + 1);
        }
        else if (i + 1 < argc)
        {
            value = argv[++i];
        }
        else
        {
            return UsageError(std::format("{} needs a value", name));
        }

        if (name == "-r" || name == "--role")
        {
            opts.role = ParseTextureRole(value);
            if (!opts.role)
                return UsageError(std::format("unknown role '{}'", value));
        }
        else if (name == "-p" || name == "--platform")
        {
            const auto platform = std::ranges::find(kPlatforms, value, &PlatformInfo::name);
            if (platform == std::end(kPlatforms))
            {
                return UsageError(std::format("unknown platform '{}'", value));
            }

            opts.platform = platform->name;
            opts.target = platform->target;
        }
        else if (name == "--optimize")
        {
            if (!std::ranges::contains(kOptimizeModes, value))
                return UsageError(std::format("unknown optimize mode '{}'", value));
            opts.optimize = value;
        }
        else if (name == "-o" || name == "--output")
        {
            opts.output = value;
        }
        else
        {
            return UsageError(std::format("unknown option '{}'", name));
        }
    }

    if (opts.input.empty())
    {
        return UsageError("no input given");
    }

    if (opts.output.empty())
    {
        opts.output = std::filesystem::path("Cooked") / opts.platform;
    }

    return opts;
}