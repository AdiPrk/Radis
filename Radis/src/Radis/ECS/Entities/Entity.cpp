#include <PCH/pch.h>

#include "Entity.h"

namespace Dog 
{
    Entity::Entity()
    {
        entities = nullptr;
        handle = entt::null;
    }

    Entity::Entity(entt::registry* registry)
        : entities(registry)
        , handle(registry->create())
    {
    }

    Entity::Entity(entt::registry* registry, entt::entity handle)
        : entities(registry)
        , handle(handle)
    {
    }

    Entity::Entity(const Entity& other)
    {
        handle = other.handle;
        entities = other.entities;
    }

    void Entity::operator=(const Entity& other)
    {
        handle = other.handle;
        entities = other.entities;
    }

    Entity::~Entity() {
        // Shouldn't get destroyed here.
        // The scene should destroy it in the scene's destructor
        // Or when asked by the user

        // scene->GetRegistry().destroy(handle);
    }

}