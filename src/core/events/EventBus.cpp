#include "core/events/EventBus.h"

namespace haocam::core {

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

void EventBus::unsubscribe(Token token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_handlers.begin(); it != m_handlers.end(); ++it) {
        auto& handlers = it->second;
        for (auto h = handlers.begin(); h != handlers.end(); ++h) {
            if (h->token == token) {
                handlers.erase(h);
                return;
            }
        }
    }
}

} // namespace haocam::core
