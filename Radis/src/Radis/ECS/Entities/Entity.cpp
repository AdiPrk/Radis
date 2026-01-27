/*****************************************************************//**
 * \file   Entity.cpp
 * \brief  Lightweight entity wrapper around EnTT handles
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "Entity.h"

namespace Radis 
{
    Entity::Entity()
    {
        mRegistry = nullptr;
        mHandle = entt::null;
    }

    Entity::Entity(entt::registry* registry)
        : mRegistry(registry)
        , mHandle(registry->create())
    {
    }

    Entity::Entity(entt::registry* registry, entt::entity mHandle)
        : mRegistry(registry)
        , mHandle(mHandle)
    {
    }

    Entity::Entity(const Entity& other)
    {
        mHandle = other.mHandle;
        mRegistry = other.mRegistry;
    }

    Entity& Entity::operator=(const Entity& other)
    {
        mHandle = other.mHandle;
        mRegistry = other.mRegistry;

        return *this;
    }

    Entity::~Entity() {
        // Shouldn't get destroyed here.
        // The scene should destroy it in the scene's destructor
        // Or when asked by the user

        // scene->GetRegistry().destroy(mHandle);
    }

}