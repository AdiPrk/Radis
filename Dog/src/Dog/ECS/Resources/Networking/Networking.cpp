#include <PCH/pch.h>
#include "Networking.h"
#include "ECS/Systems/Editor/Windows/ChatWindow.h"
#include "Engine.h"

namespace Dog {

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
            mPacketUtils.sendPacket(mPeer, CLIENT_LEAVE_PACKET);

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
        mPacketUtils.sendPacket(mPeer, STARTED_TYPING_PACKET);
    }

    void Networking::StopTyping()
    {
        mPacketUtils.sendPacket(mPeer, STOPPED_TYPING_PACKET);
    }

    void Networking::SendMessage(const std::string& message)
    {
        mPacketUtils.sendPacket(mPeer, CHAT_MESSAGE_PACKET, message.c_str());
    }

    void Networking::SetUsername(const std::string& name)
    {
        mPacketUtils.sendPacket(mPeer, CHAT_DISPLAY_NAME_PACKET, name.c_str());
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

        mPacketUtils.sendPacket(mPeer, INIT_PLAYER_PACKET);

        ENetEvent event;
        while (running) 
        {
            int eventStatus = enet_host_service(mPeer->host, &event, 10); // Wait up to 16 milliseconds for an event

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
                }
            }
            else if (eventStatus == 0) {
                // No event, sleep for a short duration to reduce CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

} // namespace Dog