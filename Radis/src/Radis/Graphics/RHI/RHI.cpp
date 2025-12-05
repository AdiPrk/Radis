#include <PCH/pch.h>
#include "RHI.h"
#include "IMesh.h"

namespace Dog
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
            DOG_INFO("Initialized RHI with OpenGL backend");
            return true;
        }
        default:
        {
            DOG_ERROR("Failed to initialize RHI: Unsupported Graphics API backend");
            return false;
        }
        }
    }
}
