/*****************************************************************//**
 * \file   ISystem.h
 * \brief  Base interface for all ECS systems
 * 
 * \author Aditya Prakash
 * \date   2026
 *********************************************************************/
#pragma once

namespace Radis
{
    class ECS;

    /**
     * \brief Abstract base class for ECS systems.
     * 
     * Systems implement game logic by processing entities with specific components.
     */
    class ISystem
    {
    public:
        explicit ISystem(const std::string& name) : mDebugName(name) {}
        virtual ~ISystem() = default;

        ISystem(const ISystem&) = delete;
        ISystem& operator=(const ISystem&) = delete;

        // --- Lifecycle Hooks ---
        virtual void Init() {}
        virtual void FrameStart() {}
        virtual void Update(float dt) {}
        virtual void FrameEnd() {}
        virtual void Exit() {}

        const std::string& GetDebugName() const { return mDebugName; }

    protected:
        friend class ECS;
        ECS* ecs = nullptr;

    private:
        std::string mDebugName;
    };

} // namespace Radis
