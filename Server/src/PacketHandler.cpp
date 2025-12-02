#include <PCH/pch.h>
#include "PacketHandler.h"
#include "PlayerManager.h"
#include "Server.h"

namespace Dog
{
    PacketHandler::PacketHandler(PacketUtils& utils) : packetUtils(utils) {}
    PacketHandler::~PacketHandler() {}

    void PacketHandler::HandlePacket(ENetPeer* peer, ENetPacket* packet, ENetHost* host, PlayerManager& playerManager)
    {
        NetMessage msg;
        if (!packetUtils.decode(packet, msg))
        {
            printf("Failed to decode incoming packet.\n");
            return;
        }

        PacketID    packetID = msg.id;
        std::string data = msg.body.value("data", std::string{});

        switch (packetID)
        {
        case INIT_PLAYER_PACKET:
        {
            std::string playerName = playerManager.GetPlayerName(peer);
            printf("Init Player %s\n", playerName.c_str());

            // Tell everyone else about this player
            packetUtils.broadcast(host, peer, INIT_PLAYER_PACKET, nlohmann::json{{"data", playerName}});

            // Tell this peer what their own name is
            packetUtils.send(peer, SELF_NAME_PACKET, nlohmann::json{{"data", playerName}});
            break;
        }

        case STARTED_TYPING_PACKET:
        {
            std::string user = playerManager.GetPlayerName(peer);
            packetUtils.broadcast(host, peer, STARTED_TYPING_PACKET, nlohmann::json{{"data", user}});
            break;
        }

        case STOPPED_TYPING_PACKET:
        {
            std::string user = playerManager.GetPlayerName(peer);
            packetUtils.broadcast(host, peer, STOPPED_TYPING_PACKET, nlohmann::json{{"data", user}});
            break;
        }

        case CHAT_MESSAGE_PACKET:
        {
            std::string message = data;
            std::string user = playerManager.GetPlayerName(peer);
            std::string full = user + ": " + message;

            packetUtils.broadcast(host, peer, CHAT_MESSAGE_PACKET, nlohmann::json{{"data", full}});
            break;
        }

        case CHAT_DISPLAY_NAME_PACKET:
        {
            std::string name = data;
            std::string oldName = playerManager.GetPlayerName(peer);

            playerManager.UpdatePlayerName(peer, name);
            std::string nameChange = oldName + "," + name;

            packetUtils.broadcast(host, peer, CHAT_DISPLAY_NAME_PACKET, nlohmann::json{{"data", nameChange}});
            break;
        }

        case CLIENT_LEAVE_PACKET:
        {
            printf("Client %s left.\n", data.c_str());
            std::string user = playerManager.GetPlayerName(peer);

            packetUtils.broadcast(host, peer, REMOVE_PLAYER_PACKET, nlohmann::json{{"data", user}});
            playerManager.PopPlayer(peer);
            break;
        }

        case EVENT_ACTION:
        {
            packetUtils.broadcast(host, peer, EVENT_ACTION, nlohmann::json{{"data", data}});
            break;
        }

        case EVENT_ACTION_UNDO:
        {
            packetUtils.broadcast(host, peer, EVENT_ACTION_UNDO, nlohmann::json{{"data", data}});
            break;
        }

        default:
        {
            printf("Unknown packet ID: %d\n", static_cast<int>(packetID));
            break;
        }
        }
    }
}
