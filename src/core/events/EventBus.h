#pragma once

// Lightweight typed publish/subscribe bus for cross-thread status events
// (performance stats, camera state changes, provider availability).
//
// Events are delivered synchronously on the publishing thread. UI layers are
// expected to marshal into the GUI thread themselves (Qt queued signals).

#include <functional>
#include <map>
#include <mutex>
#include <typeindex>
#include <vector>

namespace haocam::core {

class EventBus {
public:
    using Token = std::uint64_t;

    static EventBus& instance();

    // Subscribes to events of type E. Returns a token for unsubscribe().
    template <typename E>
    Token subscribe(std::function<void(const E&)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const Token token = ++m_nextToken;
        m_handlers[std::type_index(typeid(E))].push_back(
            {token, [handler = std::move(handler)](const void* event) {
                 handler(*static_cast<const E*>(event));
             }});
        return token;
    }

    void unsubscribe(Token token);

    // Publishes `event` to every current subscriber of its type.
    template <typename E>
    void publish(const E& event) {
        std::vector<Handler> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (auto it = m_handlers.find(std::type_index(typeid(E)));
                it != m_handlers.end()) {
                snapshot = it->second;
            }
        }
        for (const auto& handler : snapshot) handler.invoke(&event);
    }

private:
    struct Handler {
        Token token;
        std::function<void(const void*)> invoke;
    };

    EventBus() = default;

    std::mutex m_mutex;
    std::map<std::type_index, std::vector<Handler>> m_handlers;
    Token m_nextToken = 0;
};

} // namespace haocam::core
