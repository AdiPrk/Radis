#include <PCH/pch.h>
#include "Device.h"
#include "Graphics/Window/Window.h"

namespace Dog {

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

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

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
    Device::Device(Window& window) 
        : window{ window }
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();

        allocator = std::make_unique<Allocator>(*this);
    }

    Device::~Device() 
    {
        allocator.reset();

        vkDestroyCommandPool(device_, commandPool, nullptr);
        vkDestroyDevice(device_, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface_, nullptr);
        vkDestroyInstance(instance, nullptr);
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
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
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

    void Device::pickPhysicalDevice() 
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        // std::cout << "Device count: " << deviceCount << std::endl;
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::cout << "physical device: " << properties.deviceName << std::endl;
    }

    void Device::createLogicalDevice() {
        DOG_INFO("Starting logical device creation.");

        if (physicalDevice == VK_NULL_HANDLE) {
            DOG_ERROR("physicalDevice is VK_NULL_HANDLE — cannot create logical device.");
            throw std::runtime_error("physicalDevice is VK_NULL_HANDLE");
        }

        CheckIndirectDrawSupport();

        // 2) Find queue families and validate indices
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        auto INVALID_INDEX = std::numeric_limits<uint32_t>::max();
        if (indices.graphicsFamily == INVALID_INDEX) {
            DOG_ERROR("Graphics queue family not found.");
            throw std::runtime_error("Graphics queue family not found");
        }
        if (indices.presentFamily == INVALID_INDEX) {
            DOG_WARN("Present queue family not found — continuing, but present operations may fail.");
        }

        // 2.a) enumerate actual queue family count and properties to validate indices are in-range
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0) {
            DOG_ERROR("vkGetPhysicalDeviceQueueFamilyProperties returned count == 0.");
        }
        std::vector<VkQueueFamilyProperties> queueProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps.data());

        if (indices.graphicsFamily >= queueFamilyCount) {
            DOG_ERROR("graphicsFamily index out of range: {0}", indices.graphicsFamily);
        }
        if (indices.presentFamily != INVALID_INDEX && indices.presentFamily >= queueFamilyCount) {
            DOG_WARN("presentFamily index out of range: {0}", indices.presentFamily);
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
                DOG_WARN("QueueFamily {0} not present in queue properties.", queueFamily);
            }

            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // 4) Prepare requested features, but first query supported features (core and 1.2/1.3)
        DOG_INFO("Querying supported device features (core + 1.2/1.3 feature structs).");
        VkPhysicalDeviceFeatures2 supportedFeatures2{};
        supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceVulkan12Features supported12{};
        supported12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceVulkan13Features supported13{};
        supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        supported12.pNext = &supported13;
        supportedFeatures2.pNext = &supported12;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures2);

        // populate desired features (copying your requested flags)
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.multiDrawIndirect = VK_TRUE;
        deviceFeatures.tessellationShader = VK_TRUE;
        deviceFeatures.pipelineStatisticsQuery = VK_TRUE;
        deviceFeatures.logicOp = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features = {};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13Features.shaderDemoteToHelperInvocation = VK_TRUE;
        vulkan13Features.dynamicRendering = VK_TRUE;

        VkPhysicalDeviceVulkan12Features vulkan12Features = {};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.drawIndirectCount = VK_TRUE;
        vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        vulkan12Features.runtimeDescriptorArray = VK_TRUE;
        vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
        vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;

        // Compare requested features against supported features and disable unsupported ones with logs
        auto warnIfNotSupported = [&](const char* name, bool requested, bool supported, auto& outFlag) {
            if (requested && !supported) {
                DOG_WARN("{0} requested but NOT supported by the physical device. Disabling.", name);
                outFlag = VK_FALSE;
            }
            else if (requested && supported) {
                DOG_INFO("{0} requested and supported.", name);
                outFlag = VK_TRUE;
            }
            else {
                DOG_INFO("{0} not requested.", name);
                outFlag = VK_FALSE;
            }
            };

        // Core VkPhysicalDeviceFeatures (supportedFeatures2.features)
        warnIfNotSupported("samplerAnisotropy", deviceFeatures.samplerAnisotropy, supportedFeatures2.features.samplerAnisotropy, deviceFeatures.samplerAnisotropy);
        warnIfNotSupported("multiDrawIndirect", deviceFeatures.multiDrawIndirect, supportedFeatures2.features.multiDrawIndirect, deviceFeatures.multiDrawIndirect);
        warnIfNotSupported("tessellationShader", deviceFeatures.tessellationShader, supportedFeatures2.features.tessellationShader, deviceFeatures.tessellationShader);
        warnIfNotSupported("pipelineStatisticsQuery", deviceFeatures.pipelineStatisticsQuery, supportedFeatures2.features.pipelineStatisticsQuery, deviceFeatures.pipelineStatisticsQuery);
        warnIfNotSupported("logicOp", deviceFeatures.logicOp, supportedFeatures2.features.logicOp, deviceFeatures.logicOp);
        warnIfNotSupported("fillModeNonSolid", deviceFeatures.fillModeNonSolid, supportedFeatures2.features.fillModeNonSolid, deviceFeatures.fillModeNonSolid);

        // Vulkan 1.2 features
        warnIfNotSupported("drawIndirectCount", vulkan12Features.drawIndirectCount, supported12.drawIndirectCount, vulkan12Features.drawIndirectCount);
        warnIfNotSupported("shaderSampledImageArrayNonUniformIndexing", vulkan12Features.shaderSampledImageArrayNonUniformIndexing, supported12.shaderSampledImageArrayNonUniformIndexing, vulkan12Features.shaderSampledImageArrayNonUniformIndexing);
        warnIfNotSupported("runtimeDescriptorArray", vulkan12Features.runtimeDescriptorArray, supported12.runtimeDescriptorArray, vulkan12Features.runtimeDescriptorArray);
        warnIfNotSupported("descriptorBindingPartiallyBound", vulkan12Features.descriptorBindingPartiallyBound, supported12.descriptorBindingPartiallyBound, vulkan12Features.descriptorBindingPartiallyBound);
        warnIfNotSupported("descriptorBindingVariableDescriptorCount", vulkan12Features.descriptorBindingVariableDescriptorCount, supported12.descriptorBindingVariableDescriptorCount, vulkan12Features.descriptorBindingVariableDescriptorCount);

        // Vulkan 1.3 features
        warnIfNotSupported("shaderDemoteToHelperInvocation", vulkan13Features.shaderDemoteToHelperInvocation, supported13.shaderDemoteToHelperInvocation, vulkan13Features.shaderDemoteToHelperInvocation);
        warnIfNotSupported("dynamicRendering", vulkan13Features.dynamicRendering, supported13.dynamicRendering, vulkan13Features.dynamicRendering);

        // 5) Check device extensions availability
        DOG_INFO("Checking device extension support.");
        uint32_t extCount = 0;
        if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr) != VK_SUCCESS) {
            DOG_WARN("vkEnumerateDeviceExtensionProperties failed to retrieve count.");
            extCount = 0;
        }
        std::vector<VkExtensionProperties> availableExts(extCount);
        if (extCount > 0) {
            if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExts.data()) != VK_SUCCESS) {
                DOG_WARN("vkEnumerateDeviceExtensionProperties failed to retrieve extensions.");
                availableExts.clear();
            }
        }

        std::set<std::string> availableExtNames;
        for (const auto& e : availableExts) {
            availableExtNames.insert(e.extensionName);
        }
        DOG_INFO("Device exposes {0} extensions.", availableExtNames.size());

        std::vector<const char*> enabledExtensions;
        enabledExtensions.reserve(deviceExtensions.size());
        for (const char* ext : deviceExtensions) {
            if (!ext) continue;
            std::string ename(ext);
            if (availableExtNames.count(ename) == 0) {
                DOG_ERROR("Required device extension not available: {0}", ename.c_str());
                throw std::runtime_error(std::string("Missing required device extension: ") + ename);
            }
            enabledExtensions.push_back(ext);
            DOG_INFO("Will enable device extension: {0}", ename.c_str());
        }

        // 6) If validation layers are requested, verify their availability
        if (enableValidationLayers) {
            DOG_INFO("Validation layers requested — verifying availability.");
            uint32_t layerCount = 0;
            if (vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, nullptr) != VK_SUCCESS) {
                DOG_WARN("vkEnumerateDeviceLayerProperties failed to retrieve layer count.");
                layerCount = 0;
            }
            std::vector<VkLayerProperties> availableLayers(layerCount);
            if (layerCount > 0) {
                if (vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, availableLayers.data()) != VK_SUCCESS) {
                    DOG_WARN("vkEnumerateDeviceLayerProperties failed to list layers.");
                    availableLayers.clear();
                }
            }
            std::set<std::string> availableLayerNames;
            for (const auto& l : availableLayers) availableLayerNames.insert(l.layerName);

            for (const char* layer : validationLayers) {
                if (!layer) continue;
                if (!availableLayerNames.count(layer)) {
                    DOG_WARN("Requested validation layer not available at device-level: {0}", layer);
                }
                else {
                    DOG_INFO("Validation layer will be enabled: {0}", layer);
                }
            }
        }
        else {
            DOG_INFO("Validation layers not requested.");
        }

        // 7) Fill create info with validated values
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        // pNext chain for feature structures: vulkan12 -> vulkan13
        vulkan12Features.pNext = &vulkan13Features;
        createInfo.pNext = &vulkan12Features;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();

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
            throw std::runtime_error("No queue create infos prepared");
        }

        DOG_INFO("Calling vkCreateDevice...");
        VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device_);
        if (res != VK_SUCCESS) {
            DOG_ERROR("vkCreateDevice failed with error code {0}", static_cast<int>(res));
            throw std::runtime_error("failed to create logical device!");
        }
        DOG_INFO("vkCreateDevice succeeded.");

        // 8) Retrieve queues and validate
        DOG_INFO("Retrieving queues.");
        vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
        DOG_INFO("graphicsQueue_ retrieved from family {0}", indices.graphicsFamily);

        if (indices.presentFamily == INVALID_INDEX) {
            DOG_WARN("presentFamily was invalid; skipping vkGetDeviceQueue for presentQueue_.");
        }
        else {
            vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
            DOG_INFO("presentQueue_ retrieved from family {0}", indices.presentFamily);
        }

        if (device_ == VK_NULL_HANDLE) {
            DOG_ERROR("Created device_ is VK_NULL_HANDLE after vkCreateDevice (unexpected).");
            throw std::runtime_error("device_ is VK_NULL_HANDLE after vkCreateDevice");
        }

        DOG_INFO("Logical device created successfully.");
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

    void Device::createSurface() { window.CreateWindowSurface(instance, &surface_); }

    bool Device::isDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate &&
            supportedFeatures.samplerAnisotropy;
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
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

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

    void Device::CreateBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage,
        VkBuffer& buffer,
        VmaAllocation& bufferAllocation)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Allocation description for VMA
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        // Create the buffer and allocate memory using VMA
        if (vmaCreateBuffer(GetVmaAllocator(), &bufferInfo, &allocInfo, &buffer, &bufferAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("failed to create and allocate buffer using VMA!");
        }
    }

    VkCommandBuffer Device::BeginSingleTimeCommands() {
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

    void Device::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
    }

    void Device::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;  // Optional
        copyRegion.dstOffset = 0;  // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        EndSingleTimeCommands(commandBuffer);
    }

    void Device::CreateImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        VmaMemoryUsage memoryUsage,
        VkImage& image,
        VmaAllocation& imageAllocation)
    {
        // Allocation info for VMA
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        // Create the image and allocate memory using VMA
        if (vmaCreateImage(GetVmaAllocator(), &imageInfo, &allocInfo, &image, &imageAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image with VMA!");
        }
    }

} // namespace Dog