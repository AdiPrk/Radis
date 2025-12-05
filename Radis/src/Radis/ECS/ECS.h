#pragma once

#include "Systems/ISystem.h"
#include "Entities/Entity.h"

namespace Radis
{
    class Entity;
    struct IResource;

    class ECS
    {
    public:
        ECS();
        ~ECS();

        ECS(const ECS&) = delete;
        ECS& operator=(const ECS&) = delete;
        ECS(ECS&&) = delete;
        ECS& operator=(ECS&&) = delete;

        void Init();
        void FrameStart();
        void Update(float dt);
        void FrameEnd();
        void Exit();
        
        // Add System
        template<typename T>
        void AddSystem()
        {
            static_assert(std::is_base_of<ISystem, T>::value, "T must inherit from ISystem");
            mSystems.emplace_back(std::make_unique<T>());
        }

        template<typename T, typename... Args>
        void AddResource(Args&&... args) {
            static_assert(std::is_base_of<IResource, T>::value, "Resource type must inherit from Radis::IResource");

            const auto typeId = std::type_index(typeid(T));

            auto [it, inserted] = mResources.try_emplace(typeId, std::make_unique<T>(std::forward<Args>(args)...));

            if (!inserted) {
                RADIS_ERROR("Resource of type '{0}' already exists.", typeid(T).name());
            }
        }

        // Gets a pointer to the resource of the specified type.
        template<typename T>
        T* GetResource() {
            static T* cachedResource = [this]
            {
                const auto typeId = std::type_index(typeid(T));
                auto it = mResources.find(typeId);

                if (it == mResources.end()) {
                    return static_cast<T*>(nullptr);
                }

                return dynamic_cast<T*>(it->second.get());
            }();

            return cachedResource;
        }

        Entity AddEntity(const std::string& name = "");
        Entity GetEntity(const std::string& name);
        Entity CloneEntity(const Entity& entity);
        void RemoveEntity(const std::string& name);
        void RemoveEntity(Entity entity);
        void RemoveEntity(entt::entity entity);
        

        entt::registry& GetRegistry() { return mRegistry; }

    private:
        // Systems/Resources
        std::vector<std::unique_ptr<ISystem>> mSystems;
        std::unordered_map<std::type_index, std::unique_ptr<IResource>> mResources;
        
        // Entities
        entt::registry mRegistry;
        std::unordered_map<std::string, Entity> mEntityMap;
    };
}
