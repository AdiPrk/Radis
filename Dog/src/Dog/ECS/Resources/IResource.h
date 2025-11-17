#pragma once

namespace Dog
{
    struct IResource
    {
    public:
        IResource() = default;
        virtual ~IResource() = default;

        IResource(const IResource&) = delete;
        IResource& operator=(const IResource&) = delete;

        virtual void Shutdown() {}

    protected:
        friend class ECS;
        ECS* ecs = nullptr; // Reference to ECS
    };
}
