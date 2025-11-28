#ifndef DOG_COMMON_H
#define DOG_COMMON_H

#include <cstddef>

namespace Dog
{
    const size_t PACKET_BUFFER_SIZE = 256;
    //const char SERVER_IP[10] = "localhost";
    const char SERVER_IP[12] = "45.61.62.97";

    enum PacketID {
        // Player join/leave packets
        INIT_PLAYER_PACKET       = 1,
        SELF_NAME_PACKET         = 2,
        REMOVE_PLAYER_PACKET     = 3,

        // Chat packets
        STARTED_TYPING_PACKET    = 4,
        STOPPED_TYPING_PACKET    = 5,
        CHAT_MESSAGE_PACKET      = 6,
        CHAT_DISPLAY_NAME_PACKET = 7,
        CLIENT_LEAVE_PACKET      = 8,
        
        // Entity Events
        EVENT_ACTION                  = 9,
        EVENT_ACTION_UNDO             = 10,
        EVENT_ENTITY_MOVE_PACKET      = 11,
        EVENT_ENTITY_ROTATE_PACKET    = 12,
        EVENT_ENTITY_SCALE_PACKET     = 13,
        EVENT_ENTITY_TRANSFORM_PACKET = 14,
    };

    enum ConnectionStatus {
        DISCONNECTED = 0,
        CONNECTED    = 1,
        CONNECTING   = 2,
    };
}

#endif