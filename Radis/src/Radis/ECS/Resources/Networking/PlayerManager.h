#pragma once

namespace Radis
{
    class PlayerManager
    {
    public:
        PlayerManager();
        ~PlayerManager();

        void AddClient(std::string id);
        void RemoveClient(std::string id);
        void SetUsername(std::string name) { mUsername = name; }
        void ChangeClientName(const std::string& oldName, const std::string& newName);

        const std::string& GetUsername() const { return mUsername; }
        const std::unordered_map<std::string, std::string>& GetOtherClients() const { return mOtherClients; }

    private:
        std::unordered_map<std::string, std::string> mOtherClients;
        std::string mUsername;
    };
}