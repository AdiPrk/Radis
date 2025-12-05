#include <PCH/pch.h>
#include "ECS.h"

#include "Resources/IResource.h"
#include "Entities/Entity.h"
#include "Components/Components.h"

namespace Radis
{
    ECS::ECS()
        : mSystems()
        , mEntityMap()
        , mRegistry()
    {
        mResources.reserve(100);
    }

    ECS::~ECS()
    {
    }

    void ECS::Init()
    {
        // Run Init
        for (auto& resource : mResources)
        {
            resource.second->ecs = this;
        }

        for (auto& system : mSystems)
        {
            system->ecs = this;
            system->Init();
        }
    }

    void ECS::FrameStart()
    {
        PROFILE_SCOPE("ECS::FrameStart");

        for (auto& system : mSystems)
        {
            std::string profileName = system->GetDebugName() + "::FrameStart";
            PROFILE_SCOPE(profileName.c_str());
            system->FrameStart();
        }
    }

    void ECS::Update(float dt)
    {
        PROFILE_SCOPE("ECS::Update");

        for (auto& system : mSystems)
        {
            std::string profileName = system->GetDebugName() + "::Update";
            PROFILE_SCOPE(profileName.c_str());
            system->Update(dt);
        }
    }

    void ECS::FrameEnd()
    {
        PROFILE_SCOPE("ECS::FrameEnd");

        for (auto& system : mSystems | std::views::reverse)
        {
            std::string profileName = system->GetDebugName() + "::FrameEnd";
            PROFILE_SCOPE(profileName.c_str());
            system->FrameEnd();
        }
    }

    void ECS::Exit()
    {
        for (auto& system : mSystems | std::views::reverse)
        {
            system->Exit();
        }

        for (auto& resource : mResources)
        {
            resource.second->Shutdown();
        }
    }

    Entity ECS::AddEntity(const std::string& name)
    {
        Entity entity(&mRegistry);

        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();

        mEntityMap[name] = entity;

        return entity;
    }

    Entity ECS::GetEntity(const std::string& name)
    {
        auto it = mEntityMap.find(name);
        if (it == mEntityMap.end())
        {
            RADIS_WARN("Entity with name '{0}' not found!", name);
            return Entity();
        }

        return it->second;
    }

    Entity ECS::CloneEntity(const Entity& entity)
    {
        auto clone = mRegistry.create();
        entt::entity handle = entity;

        // Iterate over all component storage pools
        for (auto [id, storage] : mRegistry.storage()) {
            if (storage.contains(handle)) {
                storage.push(clone, storage.value(handle));
            }
        }

        return Entity(&mRegistry, clone);
    }

    void ECS::RemoveEntity(const std::string& name)
    {
        auto it = mEntityMap.find(name);
        if (it == mEntityMap.end())
        {
            RADIS_WARN("Entity with name '{0}' not found!", name);
            return;
        }
        mRegistry.destroy(it->second);
        mEntityMap.erase(it);
    }

    void ECS::RemoveEntity(Entity entity)
    {
        mRegistry.destroy(entity);
    }

    void ECS::RemoveEntity(entt::entity entity)
    {
        mRegistry.destroy(entity);
    }

}
