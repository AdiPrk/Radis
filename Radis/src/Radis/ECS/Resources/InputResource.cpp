/*****************************************************************//**
 * \file   InputResource.cpp
 * \brief  Input resource managing GLFW window input
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "InputResource.h"

namespace Radis
{
    InputResource::InputResource(GLFWwindow* win)
        : window(win)
    {
    }
}
