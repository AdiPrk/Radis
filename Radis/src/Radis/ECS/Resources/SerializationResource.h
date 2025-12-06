#pragma once

#include "IResource.h"

namespace Radis
{
    struct TagComponent;
    struct TransformComponent;
    struct ModelComponent;
    struct CameraComponent;
    struct AnimationComponent;
    struct LightComponent;
    
    struct SerializationResource : public IResource
    {
        entt::type_list<TagComponent, TransformComponent, ModelComponent, CameraComponent, AnimationComponent, LightComponent> types;

        void Serialize(const std::string& filepath);
        void Deserialize(const std::string& filepath);

        std::string currentScenePath = "scene.json";
    };
}
