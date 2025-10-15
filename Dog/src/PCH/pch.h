#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
#define NOMINMAX

// glfw
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// imgui
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_stdlib.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// glm
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <map>
#include <regex>
#include <future>
#include <typeindex>
#include <random>
#include <filesystem>
#include <ranges>

#include "vk_mem_alloc.h"

#define AI_SBBC_DEFAULT_MAX_BONES 500
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

// EnTT
#include "entt/entt.hpp"

// Enet
#include "enet/enet.h"

// My files
#include "Utils/Logger.h"
#include "Graphics/Vulkan/Model/AssimpGlmHelper.h"
#include "Graphics/Vulkan/Model/Animation/VQS.h"

// Shared files
#include "../../Common/src/Core.h"

// undef some dumb macros
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef SendMessage
#undef SendMessage
#endif