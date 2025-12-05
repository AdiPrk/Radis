#pragma once

namespace Dog 
{
    class ISystem
    {
    public:
        ISystem(const std::string& name) : mDebugName(name), ecs(nullptr) {}
        virtual ~ISystem() = default;

        virtual void Init() {};
        virtual void FrameStart() {};
        virtual void Update(float dt) {};
        virtual void FrameEnd() {};
        virtual void Exit() {};

        const std::string& GetDebugName() const { return mDebugName; }

    protected:
        friend class ECS;
        ECS* ecs;

    private:
        std::string mDebugName;
    };
}
