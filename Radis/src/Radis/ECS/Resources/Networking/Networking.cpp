#include <PCH/pch.h>
#include "Networking.h"
#include "ECS/Systems/Editor/Windows/ChatWindow.h"
#include "Engine.h"

namespace Radis {

    // Function definitions
    void Networking::InitializeENet()
    {
        if (enet_initialize() != 0)
        {
            fprintf(stderr, "An error occurred while initializing ENet!\n");
            return;
        }
        atexit(enet_deinitialize);
    }

    ENetPeer* Networking::CreateAndConnectClient()
    {
        InitializeENet();

        ENetHost* client;
        client = enet_host_create(NULL, 1, 1, 0, 0);

        if (client == NULL)
        {
            fprintf(stderr, "An error occurred while trying to create an ENet client host!\n");
            return nullptr;
        }

        ENetAddress address;
        ENetEvent event;

        enet_address_set_host(&address, mAddress.c_str());
        address.port = mPort;

        mPeer = enet_host_connect(client, &address, 1, 0);
        if (mPeer == NULL)
        {
            fprintf(stderr, "No available peers for initiating an ENet connection!\n");
            return nullptr;
        }

        // Wait up to 10 seconds for the connection attempt to succeed
        printf("Trying to connect to server...");
        mConnectionStatus = ConnectionStatus::CONNECTING;
        if (enet_host_service(client, &event, 10000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            printf("Connection to server succeeded.\n");
            mConnectionStatus = ConnectionStatus::CONNECTED;
        }
        else {
            enet_peer_reset(mPeer);
            fprintf(stderr, "Connection to server failed.\n");
            mPeer = nullptr;
            mConnectionStatus = ConnectionStatus::DISCONNECTED;
            return nullptr;
        }

        return mPeer;
    }

    Networking::Networking(const std::string& address, uint16_t port)
        : mPacketUtils()
        , mPacketHandler(mPacketUtils)
        , mPeer(nullptr)
        , mAddress(address)
        , mPort(port)
    {
        Init();
    }

    Networking::~Networking()
    {
        Shutdown();
    }

    void Networking::Init()
    {
        mNetworkThread = std::thread(&Networking::NetworkThread, this);
    }

    void Networking::Shutdown()
    {
        if (mPeer)
        {
            // notify server wefre leaving
            mPacketUtils.send(mPeer, CLIENT_LEAVE_PACKET);

            enet_host_flush(mPeer->host);
            enet_peer_disconnect(mPeer, 0);
        }

        running = false;

        if (mNetworkThread.joinable())
        {
            mNetworkThread.join();
        }
    }

    void Networking::StartTyping()
    {
        mPacketUtils.send(mPeer, STARTED_TYPING_PACKET);
    }

    void Networking::StopTyping()
    {
        mPacketUtils.send(mPeer, STOPPED_TYPING_PACKET);
    }

    void Networking::SendMessage(const std::string& message)
    {
        mPacketUtils.send(mPeer, CHAT_MESSAGE_PACKET, nlohmann::json{{"data", message}});
    }

    void Networking::SetUsername(const std::string& name)
    {
        mPacketUtils.send(mPeer, CHAT_DISPLAY_NAME_PACKET, nlohmann::json{{"data", name}});
        mPlayerManager.SetUsername(name);
    }

    const std::string& Networking::GetUsername() const
    {
        return mPlayerManager.GetUsername();
    }

    const std::unordered_map<std::string, std::string>& Networking::GetOtherClients() const
    {
        return mPlayerManager.GetOtherClients();
    }

    void Networking::NetworkThread()
    {
        if (!CreateAndConnectClient())
        {
            return;
        }

        // tell server to init this player
        mPacketUtils.send(mPeer, INIT_PLAYER_PACKET);

        ENetEvent event;
        while (running)
        {
            int eventStatus = enet_host_service(mPeer->host, &event, 10); // Wait up to ~10 ms for an event

            if (eventStatus > 0)
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    // Handle received packet
                    mPacketHandler.HandlePacket(mPeer, event.packet, mPlayerManager);
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    // Handle disconnection
                    printf("Disconnected from server.\n");
                    enet_host_destroy(mPeer->host);
                    enet_deinitialize();
                    return;
                }
                default:
                    break;
                }
            }
            else if (eventStatus == 0) {
                // No event, sleep for a short duration to reduce CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

} // namespace Radis
