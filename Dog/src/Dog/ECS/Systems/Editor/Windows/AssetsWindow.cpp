#include <PCH/pch.h>
#include "AssetsWindow.h"
#include "Assets/Assets.h"
#include "Graphics/Common/TextureLibrary.h"
#include "Engine.h"

namespace Dog 
{
    namespace EditorWindows
    {

		class FileBrowser
		{
		public:
			FileBrowser()
				: baseDir(Assets::AssetsDir)
			{
                currentDir = baseDir;
			}

			std::filesystem::path baseDir;
			std::filesystem::path currentDir;
		};
		
        FileBrowser browser;

		bool SameDir(const std::string& d1, const std::string& d2) {
			// dirs may or may not have trailing backslash, so just check for string minus backslash
			if (d1.empty() && d2.empty()) return true;
			if (d1.empty() || d2.empty()) return false;
			std::string d1_ = d1;
			std::string d2_ = d2;
			if (d1_.back() == '\\' || d1_.back() == '/') d1_.pop_back();
			if (d2_.back() == '\\' || d2_.back() == '/') d2_.pop_back();

			return d1_ == d2_;
		}

		void UpdateAssetsWindow(TextureLibrary* tl)
		{
			if (!tl) return;

			float flipY = static_cast<float>(Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL);
			ImVec2 uv0 = { 0.0f, 1.0f };
			ImVec2 uv1 = { 1.0f, 0.0f };

			ImGui::Begin("Assets");

			if (!SameDir(browser.currentDir.string(), browser.baseDir.string()))
			{
				if (ImGui::Button("Back"))
				{
					browser.currentDir = browser.currentDir.parent_path();
				}
			}

			static float padding = 16.0f;
			static float imageSize = 100.0f;
			float cellSize = imageSize + padding;
			ImVec2 imageSizeVec = ImVec2(imageSize, imageSize);

			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = (int)(panelWidth / cellSize);
			if (columnCount < 1)
				columnCount = 1;

			std::string currentDirName = browser.currentDir.filename().string();
			if (SameDir(currentDirName, Assets::ImagesDir) || SameDir(currentDirName, Assets::ShadersDir))
			{
				ImGui::SliderFloat("Image Size", &imageSize, 16, 512);
			}

			// --- Collect and Sort Entries ---
			std::vector<std::filesystem::directory_entry> directories;
			std::vector<std::filesystem::directory_entry> files;

			// Check if directory exists before iterating
			if (std::filesystem::exists(browser.currentDir) && std::filesystem::is_directory(browser.currentDir))
			{
				for (const auto& entry : std::filesystem::directory_iterator(browser.currentDir))
				{
					if (entry.is_directory()) {
                        std::string filename = entry.path().filename().string();
						if (SameDir(filename, Assets::EditorDir) || (SameDir(filename, "spv"))) continue;
						directories.push_back(entry);
					}
					else {
						files.push_back(entry);
					}
				}
			}

			// Sort alphabetically
			std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b) {
				return a.path().filename().string() < b.path().filename().string();
			});
			std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
				return a.path().filename().string() < b.path().filename().string();
			});
			// --- End Collect and Sort ---


			ImGui::Columns(1);

			// --- Render Directories ---
			for (const auto& entry : directories)
			{
				const auto& path = entry.path();
				std::string filename = path.filename().string();

				if (ImGui::Button(filename.c_str())) {
					browser.currentDir = path;
				}
			}

            ImGui::Columns(columnCount, 0, false);

			// --- Render Separator ---
			// Only add separator if there are both directories and files
			if (!directories.empty() && !files.empty()) {
				ImGui::Columns(1);
				ImGui::Separator();
				ImGui::Columns(columnCount, 0, false);
			}

			// --- Render Files ---
			for (const auto& entry : files)
			{
				const auto& path = entry.path();

				// This is the start of the *original* 'else' block
				if (currentDirName == "") {
					continue;
				}

				ImGui::Spacing();
				ImGui::Spacing();

				// Different behavior depending on the directory.
				if (SameDir(Assets::AssetsDir, currentDirName)) {
					// Nothing to be done, really. Just default I guess?
					// Most likely there's no files here and it never comes to this point.
					ImGui::Text(path.filename().string().c_str());
				}
				else if (SameDir(currentDirName, Assets::ImagesDir)) {
					// Get the texture from the path
					std::string fileName = path.filename().string();
					std::string filePath = path.string();

					// image name to use:
					// std::string imagePath = Assets::AssetsDir + currentDirName + "/" + fileName;

					// tex id
					void* texId = 0;
					if (auto itex = tl->GetTexture(Assets::ImagesPath + fileName))
					{
						texId = itex->GetTextureID();
					}

					bool clicked = ImGui::ImageButton(fileName.c_str(), texId, imageSizeVec, uv0, uv1);

					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
						ImGui::SetDragDropPayload("Texture2D", fileName.c_str(), fileName.size() + 1);
						ImGui::Image(texId, imageSizeVec, uv0, uv1);
						ImGui::Text(fileName.c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text(fileName.c_str());
						ImGui::EndTooltip();
					}

					// for multi-line centered text
					ImVec2 textSize = ImGui::CalcTextSize(fileName.c_str(), NULL, true, imageSize);
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (imageSize - textSize.x) * 0.5f);
					ImGui::TextWrapped(fileName.c_str());
				}
				else if (SameDir(currentDirName, Assets::ShadersDir)) {
					// Get the texture from the path
					std::string fileName = path.filename().string();
					std::string filePath = path.string();
					std::string shaderAssetName = fileName;
					shaderAssetName = shaderAssetName.substr(0, shaderAssetName.find_last_of("."));

					// tex id
					void* texId = 0;
					if (auto itex = tl->GetTexture(Assets::ImagesPath + "glslIcon.png"))
					{
						texId = itex->GetTextureID();
					}

					std::string fullPath = path.string();

					bool clicked = ImGui::ImageButton(fullPath.c_str(), texId, imageSizeVec, uv0, uv1);

					bool isImageHovered = ImGui::IsItemHovered();
					bool doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

					// get if item is double clicked
					// if (isImageHovered && doubleClicked) {
					// 	std::string shaderPath = Assets::ShadersPath + fileName;
					// 
					// 	TextEditorWrapper::MyDocument shaderDoc(fileName, true, shaderPath);
					// 	textEditor.CreateNewDocument(shaderDoc);
					// }

					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
						ImGui::SetDragDropPayload("Shader", shaderAssetName.c_str(), shaderAssetName.size() + 1);
						ImGui::Image(texId, imageSizeVec, uv0, uv1);
						ImGui::Text(shaderAssetName.c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text(shaderAssetName.c_str());
						ImGui::EndTooltip();
					}

					// for multi-line centered text
					ImVec2 textSize = ImGui::CalcTextSize(shaderAssetName.c_str(), NULL, true, imageSize);
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (imageSize - textSize.x) * 0.5f);
					ImGui::TextWrapped(shaderAssetName.c_str());
				}
				else if (SameDir(currentDirName, Assets::ScenesDir)) {
					// Get the scene from the path
					std::string fileName = path.filename().string();
					std::string filePath = path.string();
					std::string sceneAssetName = Assets::ScenesPath + fileName;

					std::string sceneName = fileName;
					sceneName = sceneName.substr(0, sceneName.find_last_of("."));

					// tex id
					void* texId = 0;
					if (auto itex = tl->GetTexture(Assets::ImagesPath + "dog.png"))
					{
						texId = itex->GetTextureID();
					}

					std::string fullPath = path.string();

					bool clicked = ImGui::ImageButton(fullPath.c_str(), texId, imageSizeVec, uv0, uv1);

					bool isImageHovered = ImGui::IsItemHovered();
					bool doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

					// get if item is double clicked
					//if (isImageHovered && doubleClicked) {
					//	std::string shaderPath = sceneAssetName;
					//
					//	TextEditorWrapper::MyDocument sceneDoc(fileName, true, shaderPath);
					//	textEditor.CreateNewDocument(sceneDoc);
					//}

					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
						ImGui::SetDragDropPayload("Scene", sceneName.c_str(), sceneName.size() + 1);
						ImGui::Image((void*)(intptr_t)texId, imageSizeVec, uv0, uv1);
						ImGui::Text(sceneName.c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text(sceneName.c_str());
						ImGui::EndTooltip();
					}

					// for multi-line centered text
					ImVec2 textSize = ImGui::CalcTextSize(sceneName.c_str(), NULL, true, imageSize);
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (imageSize - textSize.x) * 0.5f);
					ImGui::TextWrapped(sceneName.c_str());
				}
				else if (SameDir(currentDirName, Assets::ModelsDir)) {
					// Get the scene from the path
					std::string fileName = path.filename().string();
					std::string filePath = path.string();
					std::string sceneAssetName = Assets::ScenesPath + fileName;

					std::string modelName = fileName;
					modelName = modelName.substr(0, modelName.find_last_of("."));

					// tex id
					void* texId = 0;
					if (auto itex = tl->GetTexture(Assets::ImagesPath + "dogmodel.png"))
					{
						texId = itex->GetTextureID();
					}

					std::string fullPath = path.string();
                    std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

					bool clicked = ImGui::ImageButton(fullPath.c_str(), texId, imageSizeVec, uv0, uv1);

					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
						ImGui::SetDragDropPayload("Model", fullPath.c_str(), fullPath.size() + 1);
						ImGui::Image((void*)(intptr_t)texId, imageSizeVec, uv0, uv1);
						ImGui::Text(modelName.c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text(modelName.c_str());
						ImGui::EndTooltip();
					}

					// for multi-line centered text
					ImVec2 textSize = ImGui::CalcTextSize(modelName.c_str(), NULL, true, imageSize);
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (imageSize - textSize.x) * 0.5f);
					ImGui::TextWrapped(modelName.c_str());
				}

				ImGui::NextColumn();
			} // End file loop


			ImGui::Columns(1);

			ImGui::End(); // Assets
		}

	}
}
