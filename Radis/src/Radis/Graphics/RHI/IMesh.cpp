#include <PCH/pch.h>
#include "IMesh.h"
#include "Graphics/Vulkan/Core/Buffer.h"

namespace Dog
{
    int IMesh::uniqueMeshIndex = 0;

    IMesh::IMesh(bool assignID)
        : mMeshID(0)
    {
        if (assignID)
        {
            mMeshID = uniqueMeshIndex++;
        }
    }
}
