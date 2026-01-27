/*****************************************************************//**
 * \file   IResource.h
 * \brief  Base interface for ECS resources
 * 
 * \author Aditya Prakash
 * \date   2026
 *********************************************************************/
#pragma once

namespace Radis
{
    class ECS;

    /**
     * \brief Abstract base for ECS resources (global singleton data).
     * 
     * Resources are non-copyable to ensure unique ownership.
     */
    struct IResource
    {
        IResource() = default;
        virtual ~IResource() = default;

        IResource(const IResource&) = delete;
        IResource& operator=(const IResource&) = delete;

        /** Called during ECS shutdown for cleanup. */
        virtual void Shutdown() {}

    protected:
        friend class ECS;
        ECS* ecs = nullptr;
    };

} // namespace Radis
