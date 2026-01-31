/*****************************************************************//**
 * \file   InputResource.h
 * \brief  ECS Resource for Input handling
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

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
