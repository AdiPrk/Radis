/*****************************************************************//**
 * \file   Entity.h
 * \brief  Lightweight entity wrapper around EnTT handles
 * 
 * \author Aditya Prakash
 * \date   2026
 *********************************************************************/
#pragma once

namespace Radis
{
    /**
     * \brief Wrapper around an EnTT entity handle with component helpers.
     */
    class Entity
    {
    public:
        Entity();
        Entity(entt::registry* registry);
        Entity(entt::registry* registry, entt::entity handle);
        Entity(const Entity& other);
        Entity& operator=(const Entity& other);
        ~Entity();

        // --- Component Management ---
        template<typename T, typename... Args>
        T& TryAddComponent(Args&&... args)
        {
            if (!HasComponent<T>())
            {
                return AddComponent<T>(std::forward<Args>(args)...);
            }
            return GetComponent<T>();
        }

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            return mRegistry->emplace<T>(mHandle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent()
        {
            return mRegistry->get<T>(mHandle);
        }

        template<typename T>
        const T& GetComponent() const
        {
            return mRegistry->get<T>(mHandle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return mRegistry->all_of<T>(mHandle);
        }

        template<typename T>
        T* TryGetComponent()
        {
            return mRegistry->try_get<T>(mHandle);
        }

        template<typename T>
        void RemoveComponent()
        {
            mRegistry->remove<T>(mHandle);
        }

        // --- Operators ---
        bool operator==(const Entity& other) const { return mHandle == other.mHandle; }
        bool operator!=(const Entity& other) const { return mHandle != other.mHandle; }
        explicit operator bool() const { return mHandle != entt::null; }
        operator entt::entity() const { return mHandle; }
        operator uint32_t() const { return static_cast<uint32_t>(mHandle); }

    private:
        entt::registry* mRegistry = nullptr;
        entt::entity mHandle = entt::null;
    };

} // namespace Radis