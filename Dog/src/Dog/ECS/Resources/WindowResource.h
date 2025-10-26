#pragma once

#include "IResource.h"

namespace Dog
{
    class IWindow;

    struct WindowResource : public IResource
    {
        WindowResource(int w, int h, std::wstring_view name);

        std::unique_ptr<IWindow> window;
    };
}
