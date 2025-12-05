#include <PCH/pch.h>
#include "Server.h"

namespace Radis
{

    Server::Server(unsigned short port)
        : mHost(nullptr)
        , mPlayerManager()
        , mPacketUtils()
        , mPacketHandler(mPacketUtils)
    {
        if (enet_initialize() != 0)
        {
            fprintf(stderr, "An error occurred while initializing ENet.\n");
            exit(EXIT_FAILURE);
        }
        atexit(enet_deinitialize);

        ENetAddress address;
        address.host = ENET_HOST_ANY;  // Listen on all network interfaces.
        address.port = port;

        mHost = enet_host_create(&address, 32, 1, 0, 0);
        if (mHost == nullptr) 
        {
            fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
            exit(EXIT_FAILURE);
        }
        printf("Server started on port %d!\n", port);
    }

    Server::~Server()
    {
        if (mHost != nullptr)
        {
            enet_host_destroy(mHost);
        }

        enet_deinitialize();
    }

    void Server::Run() 
    {
        ENetEvent event;
        while (true) 
        {
            while (enet_host_service(mHost, &event, 10) > 0)
            {
                switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    HandleConnect(event);
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    HandleReceive(event);
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    HandleDisconnect(event);
                    break;
                default:
                    break;
                }
            }
        }
    }

    void Server::HandleConnect(ENetEvent& event)
    {
        // Send all existing players to the newly connected peer.
        for (const auto& entry : mPlayerManager.GetPlayers()) {
            // entry.first is an existing peer; send its name to the new peer.
            mPacketUtils.send(
                event.peer,
                PacketID::INIT_PLAYER_PACKET,
                nlohmann::json{ {"data", entry.second} }
            );
        }

        // Add the new player.
        std::string newPlayerName = mPlayerManager.PushPlayer(event.peer);

        // Inform all other connected peers of the new player.
        for (const auto& entry : mPlayerManager.GetPlayers()) 
        {
            if (entry.first != event.peer)
            {
                mPacketUtils.send(entry.first, PacketID::INIT_PLAYER_PACKET, nlohmann::json{{"data", newPlayerName}});
            }
        }

        printf("Client %s connected.\n", newPlayerName.c_str());
    }

    static void LogRawPacket(const ENetPacket* packet)
    {
        printf("---- RAW PACKET (%zu bytes) ----\n", packet->dataLength);

        // Print hex dump
        for (size_t i = 0; i < packet->dataLength; ++i)
        {
            printf("%02X ", packet->data[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");

        // Print ASCII (escaped)
        printf("ASCII: \"");
        for (size_t i = 0; i < packet->dataLength; ++i)
        {
            unsigned char c = packet->data[i];
            if (c >= 32 && c <= 126)
                printf("%c", c);
            else
                printf(".");
        }
        printf("\"\n");
        printf("-------------------------------\n");
    }


    void Server::HandleReceive(ENetEvent& event) 
    {
        LogRawPacket(event.packet);
        mPacketHandler.HandlePacket(event.peer, event.packet, mHost, mPlayerManager);
    }

    void Server::HandleDisconnect(ENetEvent& event) 
    {
        std::string user = mPlayerManager.GetPlayerName(event.peer);

        // Empty if client left cleanly
        if (user.empty()) return;

        printf("Client %s disconnected (2).\n", user.c_str());

        mPacketUtils.broadcast(mHost, event.peer, PacketID::REMOVE_PLAYER_PACKET, nlohmann::json{{"data", user}});
        mPlayerManager.PopPlayer(event.peer);
    }

} // namespace Radis
