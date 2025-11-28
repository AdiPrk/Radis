#pragma once

#include "../IResource.h"

namespace Dog
{
    struct NetworkingResource : public IResource
    {
        NetworkingResource(const std::string& address, uint16_t port);
        void Shutdown() override;

        std::unique_ptr<class Networking> networking;
    };
}
