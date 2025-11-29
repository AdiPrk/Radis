#pragma once

#include "PlayerManager.h"
#include "PacketHandler.h"

namespace Dog
{

    class Server {
    public:
        // Creates a server listening on the given port.
        Server(unsigned short port);
        ~Server();

        // Runs the server main loop.
        void Run();

    private:
        ENetHost* mHost;
        PlayerManager mPlayerManager;
        PacketUtils mPacketUtils;
        PacketHandler mPacketHandler;

        // Helper functions for processing ENet events.
        void HandleConnect(ENetEvent& event);
        void HandleReceive(ENetEvent& event);
        void HandleDisconnect(ENetEvent& event);
    };

}
