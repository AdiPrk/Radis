#pragma once

namespace Radis
{
    // Forward declarations
    class Model;
    class Device;
    class TextureLibrary;
    

    // Class for serializing and deserializing models
    class ModelSerializer {
    public:
        static void save(const Model& model, const std::string& filename, uint32_t hash);
        static bool load(Model& model, const std::string& filename);

        static const std::string RADIS_MODEL_FILE_PATH;
        static const std::string RADIS_MODEL_EXTENTION;

    private:
        // Magic number for format verification
        static constexpr uint32_t MAGIC_NUMBER = 0x4D4F444C; // 'MODL'
        static constexpr uint32_t VERSION = 2;

        // Validate the file header and version
        static bool validateHeader(std::ifstream& file);
    };
}
