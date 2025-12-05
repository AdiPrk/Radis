#pragma once

#include "IResource.h"

namespace Radis
{
    class IWindow;

    struct WindowResource : public IResource
    {
        WindowResource(int w, int h, std::wstring_view name);

        void Create();
        void Create(int w, int h, std::wstring_view name);
        void Cleanup();

        void Shutdown() override;

        std::unique_ptr<IWindow> window;
    };
}
