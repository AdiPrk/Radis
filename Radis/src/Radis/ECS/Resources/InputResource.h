#pragma once

#include "IResource.h"

struct GLFWwindow;

namespace Radis
{
    struct InputResource : public IResource
    {
        InputResource(GLFWwindow* win);

        GLFWwindow* window;
    };
}
