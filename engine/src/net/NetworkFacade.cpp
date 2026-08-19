#include "kizuri/net/NetworkFacade.hpp"
#include "kizuri/core/Log.hpp"

namespace kizuri {

net::NetworkSession Network::s_Session;
std::vector<net::Event> Network::s_Queue;

bool Network::Host(uint16_t port) {
    if (port == 0) port = 26000;
    if (!s_Session.Host(port)) return false;
    KZ_CORE_INFO("Network: partida aberta na porta {0}.", GetLocalPort());
    return true;
}

bool Network::Connect(const std::string& address, uint16_t port) {
    if (port == 0) port = 26000;
    return s_Session.Connect(address, port);
}

void Network::Shutdown() { s_Session.Stop(); }

bool Network::Send(uint32_t peer, const void* data, size_t size) {
    return s_Session.Send(peer, data, size);
}

void Network::Update(float dt) {
    s_Session.Update(dt, [](const net::Event& ev) {
        s_Queue.push_back(ev);
        if (s_Queue.size() > 256) s_Queue.erase(s_Queue.begin());
    });
}

bool Network::PollEvent(net::Event& out) {
    if (s_Queue.empty()) return false;
    out = std::move(s_Queue.front());
    s_Queue.erase(s_Queue.begin());
    return true;
}

}
