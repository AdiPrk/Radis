#include <PCH/pch.h>
#include "VKMath.h"

namespace Dog
{

    VkTransformMatrixKHR Dog::toTransformMatrixKHR(const glm::mat4& m)
    {
        VkTransformMatrixKHR t;
        glm::mat4 tmp = glm::transpose(m);
        memcpy(&t, glm::value_ptr(tmp), sizeof(t));
        return t;
    }

}