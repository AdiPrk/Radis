#pragma once

namespace Dog {

    // Forward Declarations
    class Networking;

    /**
     * @brief ChatWindow encapsulates the functionality of a chat interface using Dear ImGui.
     */
    class ChatWindow
    {
    public:
        static ChatWindow& Get()
        {
            static ChatWindow instance;
            return instance;
        }


        /**
         * @brief Constructs a ChatWindow instance.
         * @param networking Reference to the networking system for message handling and typing status.
         */
        ChatWindow();

        /**
         * @brief Destructor for ChatWindow.
         */
        ~ChatWindow();

        /**
         * @brief Adds a new message to the chat window.
         * @param sender Name of the message sender.
         * @param text The message text.
         * @param isRead Indicates if the message has been read.
         */
        void AddMessage(const std::string& sender, const std::string& text, bool isRead = false);

        /**
         * @brief Renders the chat window. Should be called every frame.
         */
        void Render();

        void UserStartedTyping(const std::string& user);
        void UserStoppedTyping(const std::string& user);
        void UserChangedName(const std::string& user);

        void SetNetworking(Networking* net) { networking = net; }

    private:
        Networking* networking;

        /**
         * @brief Internal structure representing a chat message.
         */
        struct ChatMessage
        {
            std::string sender;    ///< Name of the sender.
            std::string text;      ///< Message content.
            std::string timestamp; ///< Time when the message was sent.
            bool        isRead;    ///< Read receipt status.
        };

        /**
         * @brief Retrieves the current time formatted as a string.
         * @return Current time in "HH:MM" format.
         */
        std::string GetCurrentTimeString() const;

        /**
         * @brief Updates the typing status based on the input buffer.
         * Signals the networking system when typing starts or stops.
         */
        void UpdateTypingStatus();

        std::vector<ChatMessage> m_messages; ///< Collection of chat messages.

        char m_inputBuf[512]; ///< Buffer for user input.
        bool m_scrollToBottom; ///< Flag to auto-scroll to the latest message.

        bool m_isTyping; ///< Tracks if the user is currently typing.
        bool m_inputFocused;

        std::vector<std::string> m_typingUsers; ///< Collection of users currently typing.
    };

}