#include "kizuri/net/Network.hpp"
#include "kizuri/core/Log.hpp"
#include <cstring>
#include <chrono>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using SOCK = SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    using SOCK = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

namespace kizuri {
namespace net {

// Flags de pacote.
enum : uint8_t {
    FLAG_HELLO     = 1,
    FLAG_HELLO_ACK = 2,
    FLAG_DATA      = 4,
    FLAG_ACK       = 8,
};

// ---------------------------------------------------------------------------
// NetSocket
// ---------------------------------------------------------------------------
bool NetSocket::Create() {
#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        KZ_CORE_ERROR("Network: WSAStartup falhou.");
        return false;
    }
#endif
    SOCK s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        KZ_CORE_ERROR("Network: não foi possível criar o socket UDP.");
        return false;
    }
    m_Socket = (uintptr_t)s;
    SetNonBlocking(true);
    return true;
}

void NetSocket::Close() {
    if (!m_Socket) return;
#if defined(_WIN32)
    closesocket((SOCK)m_Socket);
#else
    ::close((int)m_Socket);
#endif
    m_Socket = 0;
}

bool NetSocket::Bind(uint16_t port) {
    if (!m_Socket) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind((SOCK)m_Socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        KZ_CORE_ERROR("Network: falha ao dar bind na porta {0}.", port);
        return false;
    }
    return true;
}

void NetSocket::SetNonBlocking(bool nb) {
    if (!m_Socket) return;
#if defined(_WIN32)
    u_long mode = nb ? 1 : 0;
    ioctlsocket((SOCK)m_Socket, FIONBIO, &mode);
#else
    int flags = fcntl((int)m_Socket, F_GETFL, 0);
    fcntl((int)m_Socket, F_SETFL, nb ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}

bool NetSocket::SendTo(const std::string& addr, uint16_t port, const void* data, size_t size) {
    if (!m_Socket) return false;
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    if (inet_pton(AF_INET, addr.c_str(), &to.sin_addr) != 1) {
        // Resolve hostname (localhost, etc).
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(addr.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
        to.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    int sent = ::sendto((SOCK)m_Socket, (const char*)data, (int)size, 0, (sockaddr*)&to, sizeof(to));
    return sent == (int)size;
}

bool NetSocket::Receive(void* buf, size_t cap, size_t& outSize, std::string& outAddr, uint16_t& outPort) {
    if (!m_Socket) return false;
    sockaddr_in from{};
#if defined(_WIN32)
    int fromLen = sizeof(from);
#else
    socklen_t fromLen = sizeof(from);
#endif
    int n = ::recvfrom((SOCK)m_Socket, (char*)buf, (int)cap, 0, (sockaddr*)&from, &fromLen);
    if (n <= 0) return false;
    outSize = (size_t)n;
    char ip[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
    outAddr = ip;
    outPort = ntohs(from.sin_port);
    return true;
}

// ---------------------------------------------------------------------------
// NetworkSession
// ---------------------------------------------------------------------------
bool NetworkSession::Host(uint16_t port) {
    Stop();
    if (!m_Socket.Create() || !m_Socket.Bind(port)) return false;
    m_IsHost = true;
    m_Running = true;
    m_Port = port;
    KZ_CORE_INFO("Network: host escutando na porta {0}.", port);
    return true;
}

bool NetworkSession::Connect(const std::string& address, uint16_t port) {
    Stop();
    if (!m_Socket.Create() || !m_Socket.Bind(0)) return false;
    m_IsHost = false;
    m_Running = true;
    m_ServerAddr = address;
    m_ServerPort = port;

    // Cria o "peer" do servidor e manda o HELLO.
    Peer p;
    p.Addr = address;
    p.Port = port;
    p.Connected = false;
    m_Peers.push_back(p);
    m_ServerPeerId = 1;
    SendPacket(m_Peers[0], FLAG_HELLO, nullptr, 0, false);
    KZ_CORE_INFO("Network: conectando em {0}:{1}...", address, port);
    return true;
}

void NetworkSession::Stop() {
    m_Socket.Close();
    m_Running = false;
    m_IsHost = false;
    m_Peers.clear();
    m_ServerPeerId = 0;
    m_NextPeerId = 1;
}

uint16_t NetworkSession::GetLocalPort() const {
    return m_Port;
}

bool NetworkSession::Send(uint32_t peer, const void* data, size_t size) {
    if (!m_Running || m_Peers.empty()) return false;
    size_t idx = (peer >= 1 && peer <= m_Peers.size()) ? peer - 1 : 0;
    Peer& p = m_Peers[idx];
    if (!p.Connected && m_IsHost) return false;
    SendPacket(p, FLAG_DATA, data, size, true);
    return true;
}

bool NetworkSession::Broadcast(uint16_t port, const void* data, size_t size) {
    return m_Socket.SendTo("255.255.255.255", port, data, size);
}

void NetworkSession::SendPacket(Peer& peer, uint8_t flags, const void* payload, size_t size, bool reliable) {
    uint8_t buf[1024];
    if (8 + size > sizeof(buf)) size = sizeof(buf) - 8;
    uint32_t seq = (flags == FLAG_ACK) ? peer.AckSent : peer.SeqOut;
    buf[0] = flags;
    std::memcpy(buf + 1, &seq, 4);
    std::memcpy(buf + 5, &peer.AckSent, 4);
    if (payload && size) std::memcpy(buf + 9, payload, size);

    if (reliable) {
        peer.Pending.assign(buf, buf + 9 + size);
        peer.NextAckWait = seq;
        peer.RetransmitTimer = 0.0f;
        ++peer.SeqOut;
    }
    m_Socket.SendTo(peer.Addr, peer.Port, buf, 9 + size);
}

bool NetworkSession::ParsePacket(const uint8_t* buf, size_t size, uint8_t& flags, uint32_t& seq, uint32_t& ack,
                                 const uint8_t*& payload, size_t& payloadSize) {
    if (size < 9) return false;
    flags = buf[0];
    std::memcpy(&seq, buf + 1, 4);
    std::memcpy(&ack, buf + 5, 4);
    payload = buf + 9;
    payloadSize = size - 9;
    return true;
}

void NetworkSession::HandlePacket(const uint8_t* buf, size_t size, const std::string& addr, uint16_t port,
                                  const std::function<void(const Event&)>& handler) {
    uint8_t flags;
    uint32_t seq, ack;
    const uint8_t* payload;
    size_t payloadSize;
    if (!ParsePacket(buf, size, flags, seq, ack, payload, payloadSize)) return;

    // Host: identifica o peer pelo endereço/porta (ou cria um novo).
    size_t peerIdx = SIZE_MAX;
    for (size_t i = 0; i < m_Peers.size(); ++i) {
        if (m_Peers[i].Addr == addr && m_Peers[i].Port == port) { peerIdx = i; break; }
    }
    if (m_IsHost && peerIdx == SIZE_MAX) {
        Peer p;
        p.Addr = addr;
        p.Port = port;
        p.SeqIn = seq;
        m_Peers.push_back(p);
        peerIdx = m_Peers.size() - 1;
    } else if (!m_IsHost) {
        peerIdx = 0; // cliente: só fala com o servidor
    }
    if (peerIdx == SIZE_MAX) return;

    Peer& peer = m_Peers[peerIdx];
    uint32_t peerId = (uint32_t)peerIdx + 1;

    // ACK do peer: confirma a pendência (a v1 mantém UM pacote pendente por
    // peer — os ACKs chegam junto com o próximo pacote dele).
    if (peer.Pending.size() > 0)
        peer.Pending.clear();

    // Confirma a conexão.
    if (flags & FLAG_HELLO) {
        if (m_IsHost) {
            peer.Connected = true;
            peer.SeqIn = seq;
            SendPacket(peer, FLAG_HELLO_ACK, nullptr, 0, false);
            Event ev;
            ev.Type = EventType::Connect;
            ev.Peer = peerId;
            handler(ev);
        } else {
            peer.Connected = true;
            SendPacket(peer, FLAG_HELLO_ACK, nullptr, 0, false);
        }
    } else if (flags & FLAG_HELLO_ACK) {
        peer.Connected = true;
        Event ev;
        ev.Type = EventType::Connect;
        ev.Peer = peerId;
        handler(ev);
    } else if (flags & FLAG_DATA) {
        Event ev;
        ev.Type = EventType::Data;
        ev.Peer = peerId;
        ev.Data.assign(payload, payload + payloadSize);
        handler(ev);
    }
}

void NetworkSession::Update(float dt, const std::function<void(const Event&)>& handler) {
    if (!m_Running) return;

    // Recebe tudo que chegou.
    uint8_t buf[2048];
    size_t n;
    std::string addr;
    uint16_t port;
    while (m_Socket.Receive(buf, sizeof(buf), n, addr, port))
        HandlePacket(buf, n, addr, port, handler);

    // Cliente: reenvia HELLO até conectar.
    if (!m_IsHost && !m_Peers.empty() && !m_Peers[0].Connected) {
        static float helloTimer = 0.0f;
        helloTimer += dt;
        if (helloTimer >= 1.0f) {
            helloTimer = 0.0f;
            SendPacket(m_Peers[0], FLAG_HELLO, nullptr, 0, false);
        }
    }

    // Retransmissão dos pendentes + timeout.
    for (auto& peer : m_Peers) {
        if (!peer.Connected && m_IsHost) continue;
        if (peer.Pending.size() > 0) {
            peer.RetransmitTimer += dt;
            if (peer.RetransmitTimer >= 0.1f) {
                peer.RetransmitTimer = 0.0f;
                m_Socket.SendTo(peer.Addr, peer.Port, peer.Pending.data(), peer.Pending.size());
            }
        }
    }
}

} // namespace net
} // namespace kizuri
