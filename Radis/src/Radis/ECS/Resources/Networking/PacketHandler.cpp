#include <PCH/pch.h>
#include "PacketHandler.h"
#include "PlayerManager.h"
#include "ECS/Systems/Editor/Windows/ChatWindow.h"
#include "Engine.h"

namespace Dog
{
    PacketHandler::PacketHandler(PacketUtils& utils) : packetUtils(utils) {}
    PacketHandler::~PacketHandler() {}

    void PacketHandler::HandlePacket(ENetPeer* peer, ENetPacket* packet, PlayerManager& playerManager)
    {
        NetMessage msg;
        if (!packetUtils.decode(packet, msg))
        {
            printf("Failed to decode packet on client.\n");
            return;
        }

        PacketID packetID = msg.id;
        std::string data = msg.body.value("data", std::string{});

        printf("Packet id: %d\n", static_cast<int>(packetID));

        switch (packetID)
        {
        case INIT_PLAYER_PACKET:
        {
            if (!data.empty())
            {
                playerManager.AddClient(data);
            }
            break;
        }

        case SELF_NAME_PACKET:
        {
            if (!data.empty())
            {
                playerManager.SetUsername(data);
            }
            break;
        }

        case REMOVE_PLAYER_PACKET:
        {
            if (!data.empty())
            {
                ChatWindow& chatWindow = ChatWindow::Get();
                chatWindow.AddMessage("Server", data + " has left the chat.");
                chatWindow.UserStoppedTyping(data);
                playerManager.RemoveClient(data);
            }
            break;
        }

        case CHAT_MESSAGE_PACKET:
        {
            if (!data.empty())
            {
                printf("Received message: %s\n", data.c_str());
                ChatWindow& chatWindow = ChatWindow::Get();

                std::string message = data;
                auto sep = message.find(':');

                std::string sender;
                std::string messageText;

                if (sep != std::string::npos)
                {
                    sender = message.substr(0, sep);

                    if (sep + 2 <= message.size())
                        messageText = message.substr(sep + 2);
                    else if (sep + 1 < message.size())
                        messageText = message.substr(sep + 1);
                }
                else
                {
                    sender = "Unknown";
                    messageText = message;
                }

                chatWindow.AddMessage(sender, messageText);
            }
            break;
        }

        case STARTED_TYPING_PACKET:
        {
            if (!data.empty())
            {
                ChatWindow& chatWindow = ChatWindow::Get();
                chatWindow.UserStartedTyping(data);
            }
            break;
        }

        case STOPPED_TYPING_PACKET:
        {
            if (!data.empty())
            {
                ChatWindow& chatWindow = ChatWindow::Get();
                chatWindow.UserStoppedTyping(data);
            }
            break;
        }

        case CHAT_DISPLAY_NAME_PACKET:
        {
            if (!data.empty())
            {
                ChatWindow& chatWindow = ChatWindow::Get();
                std::string nameChange = data;

                auto commaPos = nameChange.find(',');
                std::string oldName;
                std::string newName;

                if (commaPos != std::string::npos)
                {
                    oldName = nameChange.substr(0, commaPos);
                    newName = nameChange.substr(commaPos + 1);
                }
                else
                {
                    oldName = nameChange;
                    newName = nameChange;
                }

                chatWindow.AddMessage("Server", oldName + " changed their name to " + newName);
                chatWindow.UserChangedName(oldName);
                playerManager.ChangeClientName(oldName, newName);
            }
            break;
        }

        case CLIENT_LEAVE_PACKET:
        {
            // Not expected on client
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