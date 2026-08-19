#pragma once
#include "kizuri/net/Network.hpp"

namespace kizuri {

class Network {
public:

    static bool Host(uint16_t port = 26000);

    static bool Connect(const std::string& address, uint16_t port = 26000);
    static void Shutdown();
    static bool IsRunning() { return s_Session.IsRunning(); }
    static bool IsHost() { return s_Session.IsHost(); }
    static uint16_t GetLocalPort() { return s_Session.GetLocalPort(); }

    static bool Send(uint32_t peer, const void* data, size_t size);

    static void Update(float dt);

    static bool PollEvent(net::Event& out);

private:
    static net::NetworkSession s_Session;
    static std::vector<net::Event> s_Queue;
};

}
