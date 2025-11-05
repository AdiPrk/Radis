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
		
		// Helper struct to load and hold all icons
		struct AssetIcons {
			void* folder = nullptr;
			void* shader = nullptr;
			void* scene = nullptr;
			void* model = nullptr;
			void* defaultFile = nullptr;

			AssetIcons(TextureLibrary* tl) {
				if (!tl) return;

				auto GetTexID = [&](const std::string& name) -> void* {
					if (auto itex = tl->GetTexture(Assets::ImagesPath + name)) {
						return itex->GetTextureID();
					}
					return nullptr;
				};

				folder = GetTexID("folderIcon.png");
				shader = GetTexID("glslIcon.png");
				scene = GetTexID("dog.png");
				model = GetTexID("dogmodel.png");
				defaultFile = folder; // Use folder as a fallback default
			}
		};

		// Renders text centered and wrapped under an item.
		void DrawItemText(const std::string& text, float itemWidth)
		{
			// 1. Get the natural, unwrapped size of the text
			ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

			// 2. Get the current X position (at the start of the item)
			float startX = ImGui::GetCursorPosX();

			// 3. Conditionally center the text if it's narrower than the item
			if (textSize.x < itemWidth)
			{
				float offsetX = (itemWidth - textSize.x) * 0.5f;
				ImGui::SetCursorPosX(startX + offsetX);
			}

			// 4. Render the text, forcing it to wrap at the width of the item
			ImGui::PushTextWrapPos(startX + itemWidth);
			ImGui::TextUnformatted(text.c_str());
			ImGui::PopTextWrapPos();
		}

		// Draws a single thumbnail for a file or directory.
		// Returns true if the item was clicked.
		bool DrawAssetThumbnail(
			const char* id,
			void* iconID,
			float size,
			const ImVec2& uv0,
			const ImVec2& uv1,
			const std::string& displayText,
			const char* payloadType = nullptr, // Optional: for drag/drop
			const std::string& payloadData = "" // Optional: payload
		) {
			ImGui::PushID(id);

			// Draw the button
			bool clicked = ImGui::ImageButton("##thumbnail", iconID, { size, size }, uv0, uv1, { 0,0,0,0 });

			// Handle drag-and-drop source
			if (payloadType && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				ImGui::SetDragDropPayload(payloadType, payloadData.c_str(), payloadData.size() + 1);
				ImGui::Image(iconID, { size, size }, uv0, uv1); // Show preview
				ImGui::TextUnformatted(displayText.c_str());
				ImGui::EndDragDropSource();
			}

			// Handle tooltip
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(displayText.c_str());
				ImGui::EndTooltip();
			}

			// Draw the centered, wrapped text
			DrawItemText(displayText, size);

			ImGui::PopID();
			ImGui::NextColumn();

			return clicked;
		}

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

			static FileBrowser browser;

			float flipY = static_cast<float>(Engine::GetGraphicsAPI() != GraphicsAPI::OpenGL);
			ImVec2 uv0 = { 0.0f, 1.0f };
			ImVec2 uv1 = { 1.0f, 0.0f };

			AssetIcons icons(tl);

			ImGui::Begin("Assets");

			if (browser.currentDir != browser.baseDir)
			{
				if (ImGui::Button("Back"))
				{
					browser.currentDir = browser.currentDir.parent_path();
				}
				ImGui::SameLine();
			}
			ImGui::TextUnformatted(browser.currentDir.string().c_str());

			static float padding = 16.0f;
			static float thumbnailSize = 100.0f;
			float cellSize = thumbnailSize + padding;

			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = (int)(panelWidth / cellSize);
			if (columnCount < 1)
				columnCount = 1;

			std::string currentDirName = browser.currentDir.filename().string();
			if (currentDirName == Assets::ImagesDir || currentDirName == Assets::ShadersDir)
			{
				ImGui::SliderFloat("Thumbnail Size", &thumbnailSize, 16, 512);
			}

			// --- Collect and Sort Entries ---
			std::vector<std::filesystem::directory_entry> directories;
			std::vector<std::filesystem::directory_entry> files;

			if (std::filesystem::exists(browser.currentDir) && std::filesystem::is_directory(browser.currentDir))
			{
				for (const auto& entry : std::filesystem::directory_iterator(browser.currentDir))
				{
					std::string filename = entry.path().filename().string();
					if (entry.is_directory()) {
						// Filter out specific directories
						if (filename == Assets::EditorDir || filename == "spv") continue;
						directories.push_back(entry);
					}
					else {
						files.push_back(entry);
					}
				}
			}

			auto sortAlpha = [](const auto& a, const auto& b) {
				return a.path().filename().string() < b.path().filename().string();
			};
			std::sort(directories.begin(), directories.end(), sortAlpha);
			std::sort(files.begin(), files.end(), sortAlpha);


			// --- Setup columns for the grid ---
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent background
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
			ImGui::Columns(columnCount, 0, false);

			// --- Render Directories ---
			for (const auto& entry : directories)
			{
				const auto& path = entry.path();
				std::string filename = path.filename().string();

				if (DrawAssetThumbnail(path.string().c_str(), icons.folder, thumbnailSize, uv0, uv1, filename)) 
				{
					browser.currentDir = path;
				}
			}

			// --- Render Separator ---
			if (!directories.empty() && !files.empty()) {
				ImGui::Columns(1);
				ImGui::Separator();
				ImGui::Columns(columnCount, 0, false);
			}

			// --- Render Files ---
			for (const auto& entry : files)
			{
				const auto& path = entry.path();
				std::string fileName = path.filename().string();
				std::string fullPath = path.string();
				std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

				// Default asset name (e.g., for images)
				std::string assetName = fileName;
				// Asset name without extension (e.g., for shaders, scenes)
				std::string baseName = fileName.substr(0, fileName.find_last_of("."));


				if (SameDir(currentDirName, Assets::ImagesDir)) 
				{
					void* texID = icons.defaultFile; // Fallback
					if (auto itex = tl->GetTexture(Assets::ImagesPath + fileName))
					{
						texID = itex->GetTextureID();
					}
					DrawAssetThumbnail(fullPath.c_str(), texID, thumbnailSize, uv0, uv1, fileName, "Texture2D", fileName);
				}
				else if (SameDir(currentDirName, Assets::ShadersDir))
				{
					DrawAssetThumbnail(fullPath.c_str(), icons.shader, thumbnailSize, uv0, uv1, baseName, "Shader", baseName);
				}
				else if (SameDir(currentDirName, Assets::ScenesDir)) 
				{
					DrawAssetThumbnail(fullPath.c_str(), icons.scene, thumbnailSize, uv0, uv1, baseName, "Scene", baseName);
				}
				else if (SameDir(currentDirName, Assets::ModelsDir)) 
				{
					DrawAssetThumbnail(fullPath.c_str(), icons.model, thumbnailSize, uv0, uv1, baseName, "Model", fullPath);
				}
				else 
				{
					// Default fallback for other files
					DrawAssetThumbnail(fullPath.c_str(), icons.defaultFile, thumbnailSize, uv0, uv1, fileName);
				}

			} // End file loop

			// Pop styles and reset columns
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
			ImGui::Columns(1);

			ImGui::End(); // Assets
		}

	}
}
