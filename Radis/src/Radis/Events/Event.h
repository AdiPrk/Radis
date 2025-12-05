#pragma once

namespace Radis 
{
    namespace Event {
        // Window events
        struct WindowResize { int width, height; };

        // Key events
        struct KeyTrigger { int key; }; // Not used.
        struct KeyRelease { int key; }; // Not used.
        struct KeyDown { int key; };    // Not used.

        // Mouse Events
        struct MouseButtonTrigger { int button; };      // Not used.
        struct MouseButtonRelease { int button; };      // Not used.
        struct MouseDown { int button; };               // Not used.
        struct MouseMove { float x, y; };               // Not used.
        struct MouseScroll { float xOffset, yOffset; }; // Not used.

        // Scene events
        struct SceneResize { int width, height; };

        // Toolbar events
        struct PlayButtonPressed {};
        struct PauseButtonPressed {};
        struct StopButtonPressed {};
        struct StepButtonPressed {};

        // File events
        struct ImageFileDeleted { const std::string& path; };
        struct ImageFileCreated { const std::string& path; };
        struct ImageFileModified { const std::string& path; };
        struct ShaderFileDeleted { const std::string& path; };
        struct ShaderFileCreated { const std::string& path; };
        struct ShaderFileModified { const std::string& path; };
    }

    // Event manager class
    class Events {
    public:
        // This happens when the event handle (unique pointer) goes out of scope
        template <typename EventType>
        struct HandleDeleter {
            void operator()(std::function<void(const EventType&)>* listener) {
                Events::Unsubscribe<EventType>(listener);
            }
        };

        template <typename EventType>
        using Handle = std::unique_ptr<std::function<void(const EventType&)>, HandleDeleter<EventType>>;

        template <typename EventType>
        static Handle<EventType> Subscribe(std::function<void(const EventType&)> listener) {
            auto& listeners = GetListeners<EventType>();
            auto handle = std::unique_ptr<std::function<void(const EventType&)>, HandleDeleter<EventType>>(new std::function<void(const EventType&)>(listener));
            listeners.push_back(handle.get());

            DOG_INFO("Subscribed to event of type {0}.", typeid(EventType).name());

            return handle;
        }

        template <typename EventType>
        static void Unsubscribe(std::function<void(const EventType&)>* listener) {
            auto& listeners = GetListeners<EventType>();
            listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());

            DOG_INFO("Unsubscribed from event of type {0}.", typeid(EventType).name());
        }

        template <typename EventType>
        static void Publish(const EventType& event) {
            auto& listeners = GetListeners<EventType>();
            for (auto& listener : listeners) {
                (*listener)(event);
            }
        }

    private:
        template <typename EventType>
        static std::vector<std::function<void(const EventType&)>*>& GetListeners() {
            static std::unordered_map<std::type_index, std::vector<std::function<void(const EventType&)>*>> listeners;
            return listeners[std::type_index(typeid(EventType))];
        }
    };

    // Macro for subscribing to events
#define SUBSCRIBE_EVENT(EVENT_TYPE, LISTENER) \
    Events::Subscribe<EVENT_TYPE>([&](const auto& event) { LISTENER(event); })

#define UNSUBSCRIBE_EVENT(EVENT_TYPE, HANDLE) \
    (HANDLE).reset();

#define PUBLISH_EVENT(EVENT_TYPE, ...) \
    Events::Publish(EVENT_TYPE{__VA_ARGS__})

} // namespace Radis
