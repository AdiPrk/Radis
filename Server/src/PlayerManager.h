#pragma once

namespace Radis
{

    class PlayerManager {
    public:
        PlayerManager();
        ~PlayerManager();

        // Add/Remove players
        std::string PushPlayer(ENetPeer* peer);
        void PopPlayer(ENetPeer* peer);

        // Player name helpers
        void UpdatePlayerName(ENetPeer* peer, const std::string& newName);
        std::string GetPlayerName(ENetPeer* peer) const;

        const std::unordered_map<ENetPeer*, std::string>& GetPlayers() const { return mPlayers; }

    private:
        std::unordered_map<ENetPeer*, std::string> mPlayers;
        int mIdCounter;
    };

}