#pragma once

namespace Radis {

    class Entity {
    public:
        Entity();
        Entity(entt::registry* registry);
        Entity(entt::registry* registry, entt::entity handle);
        Entity(const Entity& other);
        void operator=(const Entity& other);
        ~Entity();

        template<typename T, typename... Args>
        T& TryAddComponent(Args&&... args) {
            if (!HasComponent<T>()) {
                return AddComponent<T>(std::forward<Args>(args)...);
            }
            return GetComponent<T>();
        }

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args) {
            return entities->emplace<T>(handle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent() {
            return entities->get<T>(handle);
        }

        template<typename T>
        bool HasComponent() {
            return entities->all_of<T>(handle);
        }

        template<typename T>
        T* TryGetComponent() {
            return entities->try_get<T>(handle);
        }

        template<typename T>
        void RemoveComponent() {
            entities->remove<T>(handle);
        }

        bool operator==(const Entity& other) const {
            return handle == other.handle;
        }

        bool operator!=(const Entity& other) const {
            return !operator==(other);
        }

        operator bool() const {
            return handle != entt::null;
        }

        operator entt::entity() const {
            return handle;
        }

        operator uint32_t() const {
            return (uint32_t)handle;
        }

    private:
        entt::registry* entities;
        entt::entity handle;
    };

}