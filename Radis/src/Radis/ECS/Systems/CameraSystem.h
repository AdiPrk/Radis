/*****************************************************************//**
 * \file   CameraSystem.h
 * \brief  Manages cameras
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "ISystem.h"

namespace Radis
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