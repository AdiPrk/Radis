// Dog.cpp - simple component metadata + ECS storage for testing
// Build as a DLL (x64). Export C functions.

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <mutex>
#include <memory>

// -------------------- metadata types --------------------

enum FieldType : int {
    FT_Float = 0,
    FT_Int = 1,
    FT_Bool = 2,
    FT_Vec2 = 3
};

struct FieldInfo {
    const char* name;
    int offset;
    int type;   // FieldType
    int size;
};

struct ComponentInfo {
    const char* name;
    int size;
    FieldInfo* fields; // owned statically
    int fieldCount;
};

// -------------------- component definitions --------------------

// Example component types in C++
struct Transform {
    float x, y;
};
struct Velocity {
    float vx, vy;
};

// Fields metadata (statically allocated)
static FieldInfo TransformFields[] = {
    {"x", offsetof(Transform, x), FT_Float, sizeof(float)},
    {"y", offsetof(Transform, y), FT_Float, sizeof(float)}
};

static FieldInfo VelocityFields[] = {
    {"vx", offsetof(Velocity, vx), FT_Float, sizeof(float)},
    {"vy", offsetof(Velocity, vy), FT_Float, sizeof(float)}
};

static ComponentInfo RegisteredComponents[] = {
    { "Transform", (int)sizeof(Transform), TransformFields, 2 },
    { "Velocity",  (int)sizeof(Velocity),  VelocityFields,  2 }
};

static const int RegisteredComponentCount = sizeof(RegisteredComponents) / sizeof(RegisteredComponents[0]);

// -------------------- ECS storage --------------------

// We'll store raw component memory per entity per component type.
// Use a struct to keep a stable pointer.
struct CompInstance {
    std::unique_ptr<uint8_t[]> data; // stable pointer
    int size;
    CompInstance(int s = 0) : data(nullptr), size(s) {
        if (s > 0) data.reset(new uint8_t[s]);
    }
};

static std::unordered_map<int, std::unordered_map<std::string, CompInstance>> g_entityComponents;
static std::mutex g_mutex;

static void log(const std::string& s) {
    std::cout << "[DogEngine] " << s << std::endl;
}

// -------------------- helper functions --------------------

static const ComponentInfo* findRegisteredComponent(const char* name) {
    for (int i = 0; i < RegisteredComponentCount; ++i) {
        if (strcmp(RegisteredComponents[i].name, name) == 0) return &RegisteredComponents[i];
    }
    return nullptr;
}

// -------------------- exported C API --------------------

extern "C" {

    // Get how many component types are registered
    __declspec(dllexport) int Engine_GetRegisteredComponentCount() {
        return RegisteredComponentCount;
    }

    // Get a registered component name by index
    __declspec(dllexport) const char* Engine_GetRegisteredComponentName(int index) {
        if (index < 0 || index >= RegisteredComponentCount) return "";
        return RegisteredComponents[index].name;
    }

    // Get component size
    __declspec(dllexport) int Engine_GetRegisteredComponentSize(const char* componentName) {
        auto c = findRegisteredComponent(componentName);
        return c ? c->size : 0;
    }

    // Field count for a component
    __declspec(dllexport) int Engine_GetComponentFieldCount(const char* componentName) {
        auto c = findRegisteredComponent(componentName);
        return c ? c->fieldCount : 0;
    }

    // Field info by index
    struct FieldInfoC {
        const char* name;
        int offset;
        int type;
        int size;
    };

    __declspec(dllexport) FieldInfoC Engine_GetComponentFieldInfo(const char* componentName, int index) {
        FieldInfoC out{ "", 0, 0, 0 };
        auto c = findRegisteredComponent(componentName);
        if (!c) return out;
        if (index < 0 || index >= c->fieldCount) return out;
        const FieldInfo& f = c->fields[index];
        out.name = f.name;
        out.offset = (int)f.offset;
        out.type = f.type;
        out.size = f.size;
        return out;
    }

    // Ensure entity has the component; returns pointer to component memory (stable for lifetime of component)
    __declspec(dllexport) void* Engine_GetComponentPtr(int entityId, const char* componentName) {
        std::scoped_lock lock(g_mutex);
        auto cinfo = findRegisteredComponent(componentName);
        if (!cinfo) return nullptr;
        auto& map = g_entityComponents[entityId];
        auto it = map.find(componentName);
        if (it == map.end()) {
            CompInstance inst(cinfo->size);
            // zero-init default
            memset(inst.data.get(), 0, cinfo->size);
            auto res = map.emplace(componentName, std::move(inst));
            it = res.first;
        }
        return (void*)(it->second.data.get());
    }

    // Check if entity has component
    __declspec(dllexport) int Engine_HasComponent(int entityId, const char* componentName) {
        std::scoped_lock lock(g_mutex);
        auto itent = g_entityComponents.find(entityId);
        if (itent == g_entityComponents.end()) return 0;
        auto it = itent->second.find(componentName);
        return (it != itent->second.end()) ? 1 : 0;
    }

    // Provide a list of entities that currently have any components (simple snapshot)
    // outIds: buffer (int*), maxCount: buffer capacity, outCount: written count
    __declspec(dllexport) void Engine_GetAllEntities(int* outIds, int maxCount, int* outCount) {
        std::scoped_lock lock(g_mutex);
        int i = 0;
        for (auto& kv : g_entityComponents) {
            if (i >= maxCount) break;
            outIds[i++] = kv.first;
        }
        if (outCount) *outCount = i;
    }

    // Utility: set a component field by copying bytes (for quick tests)
    __declspec(dllexport) void Engine_WriteComponentData(int entityId, const char* componentName, int offset, const void* data, int size) {
        std::scoped_lock lock(g_mutex);
        auto itent = g_entityComponents.find(entityId);
        if (itent == g_entityComponents.end()) return;
        auto it = itent->second.find(componentName);
        if (it == itent->second.end()) return;
        if (offset + size > it->second.size) return;
        memcpy(it->second.data.get() + offset, data, size);
    }

    // A simple test entrypoint which sets up a couple of entities
    __declspec(dllexport) int RunProgram(int argc, char* argv[]) {
        log("Dog.dll RunProgram called - creating sample entities");

        // entity 1: transform+velocity
        {
            void* tptr = Engine_GetComponentPtr(1, "Transform");
            void* vptr = Engine_GetComponentPtr(1, "Velocity");
            // Transform { x=0,y=0 }, Velocity { vx=1.0f, vy=0.5f }
            float x0 = 0.0f, y0 = 0.0f;
            memcpy((uint8_t*)tptr + offsetof(Transform, x), &x0, sizeof(float));
            memcpy((uint8_t*)tptr + offsetof(Transform, y), &y0, sizeof(float));
            float vx = 1.0f, vy = 0.5f;
            memcpy((uint8_t*)vptr + offsetof(Velocity, vx), &vx, sizeof(float));
            memcpy((uint8_t*)vptr + offsetof(Velocity, vy), &vy, sizeof(float));
            log("Created entity 1 with Transform+Velocity");
        }

        // entity 2: transform only
        {
            void* tptr = Engine_GetComponentPtr(2, "Transform");
            float x = 10.0f, y = 5.0f;
            memcpy((uint8_t*)tptr + offsetof(Transform, x), &x, sizeof(float));
            memcpy((uint8_t*)tptr + offsetof(Transform, y), &y, sizeof(float));
            log("Created entity 2 with Transform");
        }

        log("RunProgram finished setup (no loop here). Managed side will drive updates.");
        return 0;
    }
} // extern "C"



// #include <PCH/pch.h>
// #include "Engine.h"
// 
// extern "C" __declspec(dllexport) int RunProgram(int argc, char* argv[])
// {
//     /* These values are only used if project not launched with DogLauncher! */
//     DogLaunch::EngineSpec すぺくっす;
//     すぺくっす.name = L"ワンワン";
//     すぺくっす.width = 1280;
//     すぺくっす.height = 720;
//     すぺくっす.fps = 0;
//     すぺくっす.serverAddress = "localhost";
//     すぺくっす.serverPort = 7777;
//     すぺくっす.graphicsAPI = Dog::GraphicsAPI::Vulkan;
//     //すぺくっす.launchWithEditor = false;
// 
//     Dog::Engine Engine(すぺくっす, argc, argv);
//     return Engine.Run("scene");
// }