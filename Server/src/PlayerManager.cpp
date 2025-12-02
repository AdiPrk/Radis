#include <PCH/pch.h>
#include "PlayerManager.h"

namespace Dog
{
    PlayerManager::PlayerManager() : mIdCounter(0) {}
    PlayerManager::~PlayerManager() {}

    std::string PlayerManager::PushPlayer(ENetPeer* peer) 
    {
        mPlayers[peer] = std::to_string(mIdCounter++);
        return mPlayers[peer];
    }

    void PlayerManager::PopPlayer(ENetPeer* peer) 
    {
        auto it = mPlayers.find(peer);
        if (it != mPlayers.end()) {
            mPlayers.erase(it);
        }
    }

    std::string PlayerManager::GetPlayerName(ENetPeer* peer) const 
    {
        auto it = mPlayers.find(peer);
        if (it != mPlayers.end()) {
            return it->second;
        }
        return "";
    }

    void PlayerManager::UpdatePlayerName(ENetPeer* peer, const std::string& newName) 
    {
        mPlayers[peer] = newName;
    }
}
