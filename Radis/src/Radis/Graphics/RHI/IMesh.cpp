#include <PCH/pch.h>
#include "IMesh.h"
#include "Graphics/Vulkan/Core/Buffer.h"

namespace Radis
{
    int IMesh::uniqueMeshIndex = 0;

    IMesh::IMesh(bool assignID)
        : mMeshID(0)
        , mEBO(0)
        , mVBO(0)
        , mVAO(0)
    {
        if (assignID)
        {
            mMeshID = uniqueMeshIndex++;
        }
    }
}
