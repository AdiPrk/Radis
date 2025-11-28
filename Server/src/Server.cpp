#include <PCH/pch.h>
#include "Server.h"

namespace Dog
{

    Server::Server(unsigned short port) 
        : host(nullptr) 
        , playerManager()
        , m_PacketUtils()
        , packetHandler(m_PacketUtils)
    {
        if (enet_initialize() != 0) {
            fprintf(stderr, "An error occurred while initializing ENet.\n");
            exit(EXIT_FAILURE);
        }
        atexit(enet_deinitialize);

        ENetAddress address;
        address.host = ENET_HOST_ANY;  // Listen on all network interfaces.
        address.port = port;

        host = enet_host_create(&address, 32, 1, 0, 0);
        if (host == nullptr) {
            fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
            exit(EXIT_FAILURE);
        }
        printf("Server started on port %d!\n", port);
    }

    Server::~Server() {
        if (host != nullptr) {
            enet_host_destroy(host);
        }
        enet_deinitialize();
    }

    void Server::run() {
        ENetEvent event;
        while (true) {
            while (enet_host_service(host, &event, 10) > 0)
            {
                switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    handleConnect(event);
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    handleReceive(event);
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    handleDisconnect(event);
                    break;
                default:
                    break;
                }
            }
        }
    }

    void Server::handleConnect(ENetEvent& event) {
        // Send all existing players to the newly connected peer.
        for (const auto& entry : playerManager.getPlayers()) {
            // entry.first is an existing peer; send its name to the new peer.
            m_PacketUtils.sendPacket(event.peer, PacketID::INIT_PLAYER_PACKET, entry.second.c_str());
        }

        // Add the new player.
        std::string newPlayerName = playerManager.pushPlayer(event.peer);

        // Inform all other connected peers of the new player.
        for (const auto& entry : playerManager.getPlayers()) {
            if (entry.first != event.peer) {
                m_PacketUtils.sendPacket(entry.first, PacketID::INIT_PLAYER_PACKET, newPlayerName.c_str());
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


    void Server::handleReceive(ENetEvent& event) {
        // Delegate packet handling.
        LogRawPacket(event.packet);
        packetHandler.HandlePacket(event.peer, event.packet, host, playerManager);
    }

    void Server::handleDisconnect(ENetEvent& event) {
        std::string user = playerManager.getPlayerName(event.peer);

        // Empty if client left cleanly
        if (user.empty()) {
            return;
        }

        printf("Client %s disconnected (2).\n", user.c_str());
        m_PacketUtils.broadcastPacket(host, event.peer, PacketID::REMOVE_PLAYER_PACKET, user.c_str());
        playerManager.popPlayer(event.peer);
    }


}
