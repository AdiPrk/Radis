#include <PCH/pch.h>
#include "Device.h"
#include "Graphics/Vulkan/VulkanWindow.h"
#include "VulkanFunctions.h"

namespace Dog 
{
    // local callback functions
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {
        std::vector<std::string> blacklistedMessages = {
            "Layer VK_LAYER_OW_OVERLAY uses API version 1.2 which is older than the application specified API version of 1.4. May cause issues.",
            "Layer VK_LAYER_OW_OBS_HOOK uses API version 1.2 which is older than the application specified API version of 1.4. May cause issues.",
            "Layer VK_LAYER_OBS_HOOK uses API version 1.3 which is older than the application specified API version of 1.4. May cause issues."
        };

        for (const auto& message : blacklistedMessages) {
            if (strcmp(pCallbackData->pMessage, message.c_str()) == 0) {
                return VK_FALSE;
            }
        }

        DOG_ERROR("validation layer: {}", pCallbackData->pMessage);

        return VK_FALSE;
    }

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance,
            "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance,
            "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

    // class member functions
    Device::Device(VulkanWindow& window) 
        : window{ window }
    {
        if (volkInitialize() != VK_SUCCESS)
        {
            DOG_ERROR("Failed to initialize volk.");
            mSupportsVulkan = false;
            return;
        }

        createInstance();
        volkLoadInstance(instance); 

        mRTFuncsAvailable = vkCreateAccelerationStructureKHR &&
            vkCmdBuildAccelerationStructuresKHR &&
            vkGetAccelerationStructureBuildSizesKHR &&
            vkGetAccelerationStructureDeviceAddressKHR &&
            vkDestroyAccelerationStructureKHR &&
            vkCreateRayTracingPipelinesKHR &&
            vkGetRayTracingShaderGroupHandlesKHR &&
            vkCmdTraceRaysKHR;

        mDebugFuncsAvailable = vkSetDebugUtilsObjectNameEXT &&
            vkCmdBeginDebugUtilsLabelEXT &&
            vkCmdEndDebugUtilsLabelEXT;

        setupDebugMessenger();
        createSurface();
        
        if (!pickPhysicalDevice())
        {
            mSupportsVulkan = false;
            return;
        }
        
        if (!createLogicalDevice())
        {
            mSupportsVulkan = false;
            return;
        }

        createCommandPool();

        Allocator::Init(this);
        
        VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        mRtProperties.pNext = &mAsProperties;
        prop2.pNext = &mRtProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);
    }

    Device::~Device() 
    {
        Allocator::Destroy();

        // Destroy device-level objects first
        if (device_ != VK_NULL_HANDLE)
        {
            if (commandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device_, commandPool, nullptr);
            }

            vkDestroyDevice(device_, nullptr);
        }

        // Destroy instance-level objects
        if (instance != VK_NULL_HANDLE)
        {
            if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE)
            {
                DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            }

            if (surface_ != VK_NULL_HANDLE)
            {
                vkDestroySurfaceKHR(instance, surface_, nullptr);
            }

            vkDestroyInstance(instance, nullptr);
        }
    }

    void Device::createInstance() 
    {
        if (enableValidationLayers && !checkValidationLayerSupport()) 
        {
            enableValidationLayers = false;
            DOG_WARN("Validation layers not available!");
        }

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Dog";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Dog";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) 
        {
            VkValidationFeatureEnableEXT enables[] = {
                VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
                // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT
            };

            VkValidationFeaturesEXT validationFeatures{};
            validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validationFeatures.enabledValidationFeatureCount = std::size(enables);
            validationFeatures.pEnabledValidationFeatures = enables;

            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);

            debugCreateInfo.pNext = nullptr;             // end of chain
            validationFeatures.pNext = &debugCreateInfo; // next in chain
            //createInfo.pNext = &validationFeatures;      // start of chain
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }

        hasGflwRequiredInstanceExtensions();
    }

    bool Device::pickPhysicalDevice() 
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) 
        {
            DOG_CRITICAL("No Vulkan-supported GPUs found!");
            return false;
        }
        DOG_INFO("Found {} physical devices.", deviceCount);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            DOG_CRITICAL("Failed to find a suitable GPU!");
            return false;
        }

        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        DOG_INFO("Using GPU: {}", properties.deviceName);

        return true;
    }

    bool Device::createLogicalDevice() 
    {
        if (physicalDevice == VK_NULL_HANDLE)
        {
            DOG_ERROR("physicalDevice is VK_NULL_HANDLE - cannot create logical device.");
            return false;
        }

        CheckIndirectDrawSupport();

        // 2) Find queue families and validate indices
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        constexpr uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();
        if (indices.graphicsFamily == INVALID_INDEX)
        {
            DOG_ERROR("Graphics queue family not found.");
            return false;
        }
        if (indices.presentFamily == INVALID_INDEX)
        {
            DOG_ERROR("Present queue family not found - continuing, but present operations may fail.");
            return false;
        }

        // 2.a) enumerate actual queue family count and properties to validate indices are in-range
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0) 
        {
            DOG_ERROR("vkGetPhysicalDeviceQueueFamilyProperties returned count == 0.");
            return false;
        }
        std::vector<VkQueueFamilyProperties> queueProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps.data());

        if (indices.graphicsFamily >= queueFamilyCount) {
            DOG_ERROR("graphicsFamily index out of range: {0}", indices.graphicsFamily);
            return false;
        }
        if (indices.presentFamily != INVALID_INDEX && indices.presentFamily >= queueFamilyCount) {
            DOG_ERROR("presentFamily index out of range: {0}", indices.presentFamily);
            return false;
        }

        graphicsFamily_ = indices.graphicsFamily;
        presentFamily_ = indices.presentFamily;

        // 3) Build queue create infos
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies;
        uniqueQueueFamilies.insert(indices.graphicsFamily);
        if (indices.presentFamily != INVALID_INDEX) uniqueQueueFamilies.insert(indices.presentFamily);

        float queuePriority = 1.0f; // pointer must remain valid until vkCreateDevice returns
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            if (queueFamily >= queueProps.size()) {
                DOG_ERROR("QueueFamily {0} not present in queue properties.", queueFamily);
                return false;
            }

            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 4) Prepare requested features, but first query supported features (core and 1.2/1.3)
        VkPhysicalDeviceFeatures2 supportedFeatures2{};
        supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        VkPhysicalDeviceVulkan11Features supported11{};
        supported11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        VkPhysicalDeviceVulkan12Features supported12{};
        supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceVulkan13Features supported13{};
        supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceRobustness2FeaturesKHR robustness2Supported{};
        robustness2Supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_KHR;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR supportedAs{};
        supportedAs.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR supportedRtPipeline{};
        supportedRtPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        supportedAs.pNext = &supportedRtPipeline;
        robustness2Supported.pNext = &supportedAs;
        supported13.pNext = &robustness2Supported;
        supported12.pNext = &supported13;
        supported11.pNext = &supported12;
        supportedFeatures2.pNext = &supported11;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures2);

        bool supportsAllRequestedFeatures = true;

#define REQUEST_FEATURE(OutStruct, SupportedStruct, FeatureName) \
        if (SupportedStruct.FeatureName) { \
            OutStruct.FeatureName = VK_TRUE; \
        } else { \
            DOG_WARN("Requested feature {} is NOT supported. Disabling.", #FeatureName); \
            supportsAllRequestedFeatures = false; \
        }

        // Populate desired features (all are VK_FALSE by default)
        VkPhysicalDeviceFeatures deviceFeatures = {};
        REQUEST_FEATURE(deviceFeatures, supportedFeatures2.features, samplerAnisotropy);
        REQUEST_FEATURE(deviceFeatures, supportedFeatures2.features, multiDrawIndirect);
        REQUEST_FEATURE(deviceFeatures, supportedFeatures2.features, tessellationShader);
        REQUEST_FEATURE(deviceFeatures, supportedFeatures2.features, pipelineStatisticsQuery);
        REQUEST_FEATURE(deviceFeatures, supportedFeatures2.features, logicOp);
        REQUEST_FEATURE(deviceFeatures, supportedFeatures2.features, fillModeNonSolid);

        VkPhysicalDeviceRobustness2FeaturesKHR robustness2Features = {};
        robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_KHR;
        REQUEST_FEATURE(robustness2Features, robustness2Supported, nullDescriptor);

        VkPhysicalDeviceVulkan13Features vulkan13Features = {};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        REQUEST_FEATURE(vulkan13Features, supported13, shaderDemoteToHelperInvocation);
        REQUEST_FEATURE(vulkan13Features, supported13, dynamicRendering);
        REQUEST_FEATURE(vulkan13Features, supported13, synchronization2);

        VkPhysicalDeviceVulkan12Features vulkan12Features = {};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        REQUEST_FEATURE(vulkan12Features, supported12, drawIndirectCount);
        REQUEST_FEATURE(vulkan12Features, supported12, shaderSampledImageArrayNonUniformIndexing);
        REQUEST_FEATURE(vulkan12Features, supported12, descriptorBindingPartiallyBound);
        REQUEST_FEATURE(vulkan12Features, supported12, descriptorBindingVariableDescriptorCount);
        REQUEST_FEATURE(vulkan12Features, supported12, runtimeDescriptorArray);
        REQUEST_FEATURE(vulkan12Features, supported12, bufferDeviceAddress);
        REQUEST_FEATURE(vulkan12Features, supported12, bufferDeviceAddressCaptureReplay);
        REQUEST_FEATURE(vulkan12Features, supported12, timelineSemaphore);

        VkPhysicalDeviceVulkan11Features vulkan11Features = {};
        vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        REQUEST_FEATURE(vulkan11Features, supported11, shaderDrawParameters);

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{};
        accelFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        REQUEST_FEATURE(accelFeature, supportedAs, accelerationStructure);

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{};
        rtPipelineFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        REQUEST_FEATURE(rtPipelineFeature, supportedRtPipeline, rayTracingPipeline);
        REQUEST_FEATURE(rtPipelineFeature, supportedRtPipeline, rayTracingPipelineTraceRaysIndirect);
        REQUEST_FEATURE(rtPipelineFeature, supportedRtPipeline, rayTraversalPrimitiveCulling);

#undef REQUEST_FEATURE

        if (!supportsAllRequestedFeatures) 
        {
            DOG_ERROR("Not all requested features are supported by the physical device.");
            return false;
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        
        accelFeature.pNext = &rtPipelineFeature;
        robustness2Features.pNext = &accelFeature;
        vulkan13Features.pNext = &robustness2Features;
        vulkan12Features.pNext = &vulkan13Features;
        vulkan11Features.pNext = &vulkan12Features;
        createInfo.pNext = &vulkan11Features;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        // Sanity checks before vkCreateDevice
        if (createInfo.queueCreateInfoCount == 0) {
            DOG_ERROR("No queue create infos prepared; cannot create device.");
            return false;
        }

        VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device_);
        if (res != VK_SUCCESS) {
            DOG_ERROR("vkCreateDevice failed with error code {0}", static_cast<int>(res));
            return false;
        }

        // 8) Retrieve queues
        vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);

        DOG_INFO("Logical device created successfully.");
        return true;
    }


    void Device::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = FindPhysicalQueueFamilies();

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        poolInfo.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void Device::CheckIndirectDrawSupport()
    {
        VkPhysicalDeviceVulkan12Features supportedFeatures = {};
        supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &supportedFeatures;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        // Check for specific feature support
        if (!supportedFeatures.drawIndirectCount) {
            DOG_ERROR("drawIndirectCount is not supported on this GPU!");
        }
    }

    void Device::createSurface() { window.CreateVulkanSurface(instance, &surface_); }

    bool Device::isDeviceSuitable(VkPhysicalDevice device)
    {
        DOG_INFO("Evaluating device suitability: {}", [&]() {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            return std::string(deviceProperties.deviceName);
        }());

        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        if (indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy) 
        {
            DOG_INFO("Device is suitable!");
        }
        else 
        {
            DOG_WARN("Device is NOT suitable.");
            DOG_WARN("Indices complete: {}, Extensions supported: {}, Swap chain adequate: {}, Sampler anisotropy: {}",
                indices.isComplete() ? "Yes" : "No",
                extensionsSupported ? "Yes" : "No",
                swapChainAdequate ? "Yes" : "No",
                supportedFeatures.samplerAnisotropy ? "Yes" : "No"
            );
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    void Device::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
    {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr;  // Optional
    }

    void Device::setupDebugMessenger() 
    {
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    bool Device::checkValidationLayerSupport() 
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> Device::getRequiredExtensions() 
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = window.GetRequiredVulkanExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void Device::hasGflwRequiredInstanceExtensions() 
    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        //std::cout << "available extensions:" << std::endl;
        std::unordered_set<std::string> available;
        for (const auto& extension : extensions) {
            //std::cout << "\t" << extension.extensionName << std::endl;
            available.insert(extension.extensionName);
        }

        //std::cout << "required extensions:" << std::endl;
        auto requiredExtensions = getRequiredExtensions();
        for (const auto& required : requiredExtensions) {
            //std::cout << "\t" << required << std::endl;
            if (available.find(required) == available.end()) {
                throw std::runtime_error("Missing required glfw extension");
            }
        }
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &extensionCount,
            availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        // print all missing extensions
        for (const auto& ext : requiredExtensions) {
            DOG_WARN("Missing required device extension: {}", ext);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                indices.graphicsFamilyHasValue = true;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
                indices.presentFamilyHasValue = true;
            }
            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainSupportDetails Device::querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device,
                surface_,
                &presentModeCount,
                details.presentModes.data());
        }
        return details;
    }

    VkFormat Device::FindSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (
                tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported format!");
    }

    uint32_t Device::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    VkCommandBuffer Device::BeginSingleTimeCommands()
    {
        // Allocate the command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        VkResult result = vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer!");
        }

        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer!");
        }

        return commandBuffer;
    }

    void Device::EndSingleTimeCommands(VkCommandBuffer commandBuffer) 
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
    }

    void Device::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
    {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;  // Optional
        copyRegion.dstOffset = 0;  // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(commandBuffer);
    }

    void Device::StartDebugLabel(VkCommandBuffer commandBuffer, const char* labelName, glm::vec4 c)
    {
        if (!mDebugFuncsAvailable) return;
        VkDebugUtilsLabelEXT s{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, labelName, { c.r, c.g, c.b, c.a } };
        vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &s);
    }

    void Device::EndDebugLabel(VkCommandBuffer commandBuffer)
    {
        if (!mDebugFuncsAvailable) return;
        vkCmdEndDebugUtilsLabelEXT(commandBuffer);
    }

} // namespace Dog