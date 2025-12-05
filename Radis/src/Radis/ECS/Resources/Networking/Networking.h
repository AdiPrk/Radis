#pragma once

#include <enet/enet.h>
#include "PacketHandler.h"
#include "PlayerManager.h"

namespace Dog
{
    class Networking
    {
    public:
        Networking(const std::string& address, uint16_t port);
        ~Networking();

        void Init();
        void Shutdown();

        void StartTyping();
        void StopTyping();
        void SendMessage(const std::string& message);
        void SetUsername(const std::string& name);

        const std::string& GetUsername() const;
        bool IsConnected() const { return mConnectionStatus == ConnectionStatus::CONNECTED; }
        ConnectionStatus GetStatus() { return mConnectionStatus; }
        const std::unordered_map<std::string, std::string>& GetOtherClients() const;

    private:
        void InitializeENet();
        ENetPeer* CreateAndConnectClient();
        void NetworkThread();

        // ENet peer
        ENetPeer* mPeer;

        // Thread
        std::thread mNetworkThread;
        bool running = true;

        // Status
        ConnectionStatus mConnectionStatus = ConnectionStatus::DISCONNECTED;
        PacketHandler mPacketHandler;
        PacketUtils mPacketUtils;
        PlayerManager mPlayerManager;

        // Connection info
        std::string mAddress;
        uint16_t mPort;
    };


}