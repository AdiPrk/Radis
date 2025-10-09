#pragma once

#include "IResource.h"

namespace Dog
{
    class Window;

    struct WindowResource : public IResource
    {
        WindowResource(int w, int h, std::wstring_view name);

        std::unique_ptr<Window> window;
    };
}
