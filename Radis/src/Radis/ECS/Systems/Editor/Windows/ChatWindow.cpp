#include <PCH/pch.h>
#include "ChatWindow.h"
#include "Engine.h"
#include "ECS/Resources/Networking/Networking.h"

namespace Radis
{
    // Example message struct for clarity:
    struct ChatMessage
    {
        std::string sender;
        std::string text;
        std::string timestamp;
        bool        isRead; // For read-receipts
    };

    std::string ChatWindow::GetCurrentTimeString() const
    {
        // Get current time as time_point
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm;
        localtime_s(&local_tm, &now_time);
        std::stringstream ss;
        ss << std::put_time(&local_tm, "%H:%M"); // Format: HH:MM

        return ss.str();
    }

    // Constructor: Initializes member variables
    ChatWindow::ChatWindow()
        : m_scrollToBottom(false)
        , m_isTyping(false)
        , m_inputFocused(false)
    {
        m_inputBuf[0] = '\0';

        AddMessage("Server", "Welcome to the chat!\nUse /user <name> to change your display name!");
    }

    ChatWindow::~ChatWindow()
    {
    }

    // Adds a new message to the chat window
    void ChatWindow::AddMessage(const std::string& sender, const std::string& text, bool isRead)
    {
        ChatMessage msg;
        msg.sender = sender;
        msg.text = text;
        msg.timestamp = GetCurrentTimeString();
        msg.isRead = isRead;

        m_messages.push_back(msg);
        m_scrollToBottom = true; // Trigger auto-scroll to the latest message
    }

    // Updates typing status and signals networking system accordingly
    void ChatWindow::UpdateTypingStatus()
    {
        // Determine if the user is currently typing (more than one character)
        bool currentlyTyping = (strlen(m_inputBuf) > 0);

        // If the input is focused and the user is typing
        if (m_inputFocused && currentlyTyping)
        {
            if (!m_isTyping)
            {
                // User started typing
                networking->StartTyping();
                m_isTyping = true;
            }
        }
        else
        {
            if (m_isTyping)
            {
                // User stopped typing either by clearing input or losing focus
                networking->StopTyping();
                m_isTyping = false;
            }
        }
    }

    // Renders the chat window using Dear ImGui
    void ChatWindow::Render()
    {
        if (!networking)
        {
            ImGui::Begin("Chat");
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Networking disabled.");
            ImGui::End();
            return;
        }

        // Begin the chat window
        ImGui::Begin("Chat");

        ConnectionStatus status = networking->GetStatus();
        if (status == ConnectionStatus::CONNECTING)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Connecting to server...");
            ImGui::End();
            return;
        }
        else if (status == ConnectionStatus::DISCONNECTED)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Not connected to server.");
            ImGui::End();
            return;
        }

        // Display online users bar
        auto onlineUsers = networking->GetOtherClients();
        if (onlineUsers.empty())
            ImGui::Text("Nobody currently online.");
        else {
            ImGui::Text("%d people online.", (int)onlineUsers.size() + 1);

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("You");
                for (const auto& user : onlineUsers)
                {
                    ImGui::TextUnformatted(user.first.c_str());
                }
                ImGui::EndTooltip();
            }
        }


        // Engine::Get().GetEditor().CaptureInput(ImGui::IsWindowFocused());

        // Handle global Enter key to focus the input box if no other item is active
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter), false) &&
            !ImGui::IsAnyItemActive())
        {
            ImGui::SetKeyboardFocusHere();
        }

        // Define the size for the scrollable message region
        float footerHeightToReserve = ImGui::GetFrameHeightWithSpacing();
        ImVec2 childSize = ImVec2(0, -footerHeightToReserve);
        ImGui::BeginChild("ScrollingRegion", childSize, false, ImGuiWindowFlags_HorizontalScrollbar);

        // Wrap text within the available content region width
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);

        // Render each message
        for (const auto& msg : m_messages)
        {
            // Color-code the sender's name
            if (msg.sender == "Adi")
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 128, 128, 255));  // Reddish
            else if (msg.sender == "Travis")
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 255, 128, 255));  // Greenish
            else if (msg.sender == "Server")
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 128, 255));  // Yellowish
            else
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 128, 255, 255));  // Blueish

            // Display timestamp and sender
            ImGui::Text("[%s] %s:", msg.timestamp.c_str(), msg.sender.c_str());
            ImGui::PopStyleColor();

            // Indent and display the message text
            ImGui::Indent(20.0f);
            ImGui::TextWrapped("%s", msg.text.c_str());

            // Optionally display read receipt
            if (msg.isRead)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(Read)");
            }

            ImGui::Unindent(20.0f);
            ImGui::Separator();
        }

        // Auto-scroll to the bottom if new messages have been added
        if (m_scrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scrollToBottom = false;
        }

        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        // Display typing indicator based on networking status
        if (!m_typingUsers.empty())
        {
            std::string typingIndicator;
            for (size_t i = 0; i < m_typingUsers.size(); ++i)
            {
                typingIndicator += m_typingUsers[i];
                if (i < m_typingUsers.size() - 1)
                    typingIndicator += ", ";
            }

            if (m_typingUsers.size() == 1)
                typingIndicator += " is typing...";
            else
                typingIndicator += " are typing...";

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", typingIndicator.c_str());
        }

        // Define input box width relative to the available content region
        float inputWidth = ImGui::GetContentRegionAvail().x - 80.0f; // Reserve space for the Send button

        // Input text with placeholder
        m_inputFocused = false;
        ImGui::PushItemWidth(inputWidth);
        if (ImGui::InputTextWithHint("##ChatInput",
            "Press enter to chat",
            m_inputBuf,
            IM_ARRAYSIZE(m_inputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            // User pressed Enter while focused on the input
            if (strlen(m_inputBuf) > 0)
            {
                // check if input has /user command
                if (m_inputBuf[0] == '/')
                {
                    std::string command = m_inputBuf;
                    if (command.find("/user") != std::string::npos)
                    {
                        if (command == "/user" || command.length() < 7) {
                            AddMessage("Server", "Please specify a display name after /user.");
                        }
                        else if (command.length() > 20 - 6)
                        {
                            AddMessage("Server", "Display name must be 14 characters or less.");
                        }
                        else {
                            std::string name = command.substr(6);
                            networking->SetUsername(name);
                            AddMessage("Server", "Your display name has been changed to: " + name);
                        }
                    }
                    else {
                        AddMessage("Server", "Unknown command: " + command);
                    }
                }
                else {
                    // Add the message to the chat window
                    std::string username = networking->GetUsername();
                    AddMessage(username, m_inputBuf, false);

                    // Send the message through networking
                    networking->SendMessage(m_inputBuf);
                }

                // Clear the input buffer
                m_inputBuf[0] = '\0';
            }
            // Re-focus the input box for continued typing
            ImGui::SetKeyboardFocusHere(-1);
        }
        if (ImGui::IsItemFocused()) m_inputFocused = true;

        ImGui::PopItemWidth();

        ImGui::SameLine();

        // Send button
        if (ImGui::Button("Send"))
        {
            if (strlen(m_inputBuf) > 0)
            {
                // Add the message to the chat window
                AddMessage("You", m_inputBuf, false);

                // Send the message through networking
                networking->SendMessage(m_inputBuf);

                // Clear the input buffer
                m_inputBuf[0] = '\0';
            }
            // Re-focus the input box for continued typing
            ImGui::SetKeyboardFocusHere(-1);
        }

        // Update typing status based on the current input buffer
        UpdateTypingStatus();

        // End the chat window
        ImGui::End();
    }

    void ChatWindow::UserStartedTyping(const std::string& user)
    {
        auto it = std::find(m_typingUsers.begin(), m_typingUsers.end(), user);
        if (it == m_typingUsers.end())
        {
            m_typingUsers.push_back(user);
        }
    }

    void ChatWindow::UserStoppedTyping(const std::string& user)
    {
        auto it = std::find(m_typingUsers.begin(), m_typingUsers.end(), user);
        if (it != m_typingUsers.end())
        {
            m_typingUsers.erase(it);
        }
    }

    void ChatWindow::UserChangedName(const std::string& user)
    {
        UserStoppedTyping(user);
    }

}