/*****************************************************************//**
 * \file   NetworkingResource.cpp
 * \brief  Networking resource wrapper for ECS
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "NetworkingResource.h"
#include "Networking.h"

#include "ECS/Systems/Editor/Windows/ChatWindow.h"

namespace Radis
{
    NetworkingResource::NetworkingResource(const std::string& address, uint16_t port)
    {
        networking = std::make_unique<Networking>(address, port);
        ChatWindow::Get().SetNetworking(networking.get());
    }

    void NetworkingResource::Shutdown()
    {
        networking.reset();
    }
}
