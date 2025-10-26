#pragma once

#include "../IWindow.h"

namespace Dog 
{
	class VulkanWindow : public IWindow {
	public:
		VulkanWindow(int w, int h, std::wstring_view name);
		~VulkanWindow();

		VulkanWindow(const VulkanWindow&) = delete;
		VulkanWindow& operator=(const VulkanWindow&) = delete;

		void CreateVulkanSurface(VkInstance instance, VkSurfaceKHR* surface);
        const char** GetRequiredVulkanExtensions(uint32_t* count) const override;

	private:
		void InitializeContext();
		void SwapBuffers() override {}
	};

} // namespace Dog
