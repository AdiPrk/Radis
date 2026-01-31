/*****************************************************************//**
 * \file   RHI.cpp
 * \brief  Implementation of the RHI (Rendering Hardware Interface)
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "RHI.h"
#include "Mesh.h"

namespace Radis
{
    bool RHI::Initialize(GraphicsAPI backend)
    {
        switch (backend)
        {
        case GraphicsAPI::Vulkan:
        {
            
            return true;
        }
        case GraphicsAPI::OpenGL:
        {
            RADIS_INFO("Initialized RHI with OpenGL backend");
            return true;
        }
        default:
        {
            RADIS_ERROR("Failed to initialize RHI: Unsupported Graphics API backend");
            return false;
        }
        }
    }
}
