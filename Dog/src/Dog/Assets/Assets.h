#pragma once

namespace Dog
{
	class Assets {
	public:
		inline static const std::string AssetsDir = "Assets/";

		inline static const std::string EditorDir = "editor/";
		inline static const std::string ShadersDir = "shaders/";
		inline static const std::string ImagesDir = "textures/";
		inline static const std::string ScenesDir = "scenes/";
		inline static const std::string FontsDir = "Fonts/";
		inline static const std::string ModelsDir = "Models/";
		inline static const std::string ModelTexturesDir = "Models/ModelTextures/";

		inline static const std::string EditorPath = AssetsDir + EditorDir;
		inline static const std::string ShadersPath = AssetsDir + ShadersDir;
		inline static const std::string ImagesPath = AssetsDir + ImagesDir;
		inline static const std::string ScenesPath = AssetsDir + ScenesDir;
        inline static const std::string FontsPath = AssetsDir + FontsDir;
        inline static const std::string ModelsPath = AssetsDir + ModelsDir;
        inline static const std::string ModelTexturesPath = AssetsDir + ModelTexturesDir;
	};
}
