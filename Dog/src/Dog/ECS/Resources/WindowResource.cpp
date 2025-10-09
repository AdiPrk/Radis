#include <PCH/pch.h>
#include "WindowResource.h"
#include "Graphics/Window/Window.h"

namespace Dog
{
    WindowResource::WindowResource(int w, int h, std::wstring_view name)
        : window(std::make_unique<Window>(w, h, name))
    {
    }
}

