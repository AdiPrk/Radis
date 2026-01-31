/*****************************************************************//**
 * \file   PlayerManager.cpp
 * \brief  Manages a list of connected players in a multiplayer session
 * 
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "PlayerManager.h"

namespace Radis
{
    PlayerManager::PlayerManager() {}
    PlayerManager::~PlayerManager() {}

    void PlayerManager::AddClient(std::string name)
    {
        mOtherClients[name] = name;
        printf("Init player %s\n", name.c_str());
    }

    void PlayerManager::RemoveClient(std::string id)
    {
        auto it = mOtherClients.find(id);
        if (it != mOtherClients.end())
        {
            mOtherClients.erase(it);
            printf("Remove player %s\n", id.c_str());
        }
    }

    void PlayerManager::ChangeClientName(const std::string& oldName, const std::string& newName)
    {
        // remove if it's in there and replace with new
        auto it = mOtherClients.find(oldName);
        if (it != mOtherClients.end())
        {
            mOtherClients.erase(it);
            mOtherClients[newName] = newName;
        }
    }
}