#pragma once

#include "ISystem.h"

namespace Dog
{
    class CameraSystem : public ISystem
    {
    public:
        CameraSystem() : ISystem("CameraSystem") {};
        ~CameraSystem() {}

        void Update(float dt);

    private:
        // Camera control settings
        float mMoveSpeed{ 20.f };
        float mMouseSensitivity{ 0.15f };

        float mYaw{ 0.0f };
        float mPitch{ 0.0f };
    };
}