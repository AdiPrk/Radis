#include <PCH/pch.h>
#include "RHI.h"
#include "IMesh.h"

namespace Radis
{
    bool RHI::RHI_Init(GraphicsAPI backend)
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
