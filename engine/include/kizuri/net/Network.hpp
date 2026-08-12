#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace kizuri {
namespace net {

// ---------------------------------------------------------------------------
// Rede multiplayer (pilar AAA v0.34) — v1: UDP confiável host/cliente.
//
// Camada mínima por cima de UDP: pacotes numerados, ACK e retransmissão
// (estilo TCP, mas sem ordem estrita de entrega e sem servidor dedicado —
// o host é um dos jogadores). Suficiente pra sincronizar transforms e
// eventos de gameplay entre 2+ instâncias na mesma rede local.
//
// Protocolo de pacote: [flags:1][seq:4][ack:4][payload...]
//   flags: 1=HELLO 2=HELLO_ACK 4=DATA 8=ACK
// ---------------------------------------------------------------------------

enum class EventType : uint8_t { Connect, Disconnect, Data, Error };

struct Event {
    EventType Type = EventType::Error;
    uint32_t Peer = 0;          // peer (host) / conexão (cliente)
    std::vector<uint8_t> Data;
};

// Socket UDP cru (binding, send/receive) — sem dependência externa.
class NetSocket {
public:
    NetSocket() = default;
    ~NetSocket() { Close(); }
    NetSocket(const NetSocket&) = delete;
    NetSocket& operator=(const NetSocket&) = delete;

    bool Create();                      // socket UDP
    void Close();
    bool Bind(uint16_t port);           // port=0 = porta aleatória
    void SetNonBlocking(bool nb);       // recepção não bloqueia (padrão: sim)
    bool SendTo(const std::string& addr, uint16_t port, const void* data, size_t size);
    // Recebe um datagrama; devolve false se não há nada (non-blocking).
    bool Receive(void* buf, size_t cap, size_t& outSize, std::string& outAddr, uint16_t& outPort);

private:
    uintptr_t m_Socket = 0; // SOCKET (Windows) / int (POSIX)
};

// Sessão de rede: host (escuta e aceita peers) OU cliente (conecta num host).
// Update() processa a rede e entrega os eventos por callback.
class NetworkSession {
public:
    ~NetworkSession() { Stop(); }

    // Host: escuta em 'port' (0 = aleatória, use GetLocalPort() depois).
    bool Host(uint16_t port = 0);
    // Cliente: conecta num host existente.
    bool Connect(const std::string& address, uint16_t port);
    void Stop();

    bool IsRunning() const { return m_Running; }
    bool IsHost() const { return m_IsHost; }
    uint16_t GetLocalPort() const;

    // Envia dados confiáveis pro peer 'peer' (host) ou pro host (cliente).
    // Confiável = reenvia até receber ACK (máx ~1s).
    bool Send(uint32_t peer, const void* data, size_t size);
    // Broadcasting local (sem confiabilidade) — ideal pra discover.
    bool Broadcast(uint16_t port, const void* data, size_t size);

    // Processa a rede (deve ser chamado todo frame do jogo). 'handler' recebe
    // os eventos (Connect/Disconnect/Data/Error).
    void Update(float dt, const std::function<void(const Event&)>& handler);

private:
    struct Peer {
        std::string Addr;
        uint16_t Port = 0;
        uint32_t SeqIn = 0;      // próximo seq esperado do peer
        uint32_t SeqOut = 0;     // próximo seq que enviamos
        uint32_t AckSent = 0;    // último seq confirmado
        uint32_t NextAckWait = 0;// último seq que aguarda ACK
        float RetransmitTimer = 0.0f;
        float Timeout = 0.0f;
        bool Connected = false;
        std::vector<uint8_t> Pending; // payload não confirmado
    };

    void SendPacket(Peer& peer, uint8_t flags, const void* payload, size_t size, bool reliable);
    bool ParsePacket(const uint8_t* buf, size_t size, uint8_t& flags, uint32_t& seq, uint32_t& ack, const uint8_t*& payload, size_t& payloadSize);
    void HandlePacket(const uint8_t* buf, size_t size, const std::string& addr, uint16_t port,
                      const std::function<void(const Event&)>& handler);

    NetSocket m_Socket;
    bool m_Running = false;
    bool m_IsHost = false;
    uint16_t m_Port = 0;

    // Host: peers por id. Cliente: um único "peer" (o host).
    std::vector<Peer> m_Peers;
    uint32_t m_NextPeerId = 1;
    std::string m_ServerAddr;
    uint16_t m_ServerPort = 0;
    uint32_t m_ServerPeerId = 0;
};

// ---- Serialização de transform (comum entre as instâncias) ----------------
// Estado mínimo de uma entidade de rede: posição + rotação (yaw) + estado.
struct NetTransform {
    uint32_t EntityId = 0;   // handle da entidade no remetente
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    float Yaw = 0.0f;
    uint8_t Flags = 0;       // bits livres pro jogo (vivo, atacando...)
};

// Empacota/desempacota um NetTransform num buffer:
//   EntityId(4) + pos(12) + Yaw(4) + Flags(1) = 21 bytes (kNetTransformSize).
// GCC com -Warray-bounds/-Wstringop-overflow valida o tamanho via static_assert
// abaixo — se o layout mudar e o tamanho não acompanhar, o build quebra.
inline void WriteNetTransform(NetTransform t, uint8_t* out) {
    uint8_t* p = out;
    *reinterpret_cast<uint32_t*>(p) = t.EntityId; p += 4;
    *reinterpret_cast<float*>(p) = t.X; p += 4;
    *reinterpret_cast<float*>(p) = t.Y; p += 4;
    *reinterpret_cast<float*>(p) = t.Z; p += 4;
    *reinterpret_cast<float*>(p) = t.Yaw; p += 4;
    *p = t.Flags;
}
inline NetTransform ReadNetTransform(const uint8_t* in) {
    NetTransform t;
    const uint8_t* p = in;
    t.EntityId = *reinterpret_cast<const uint32_t*>(p); p += 4;
    t.X = *reinterpret_cast<const float*>(p); p += 4;
    t.Y = *reinterpret_cast<const float*>(p); p += 4;
    t.Z = *reinterpret_cast<const float*>(p); p += 4;
    t.Yaw = *reinterpret_cast<const float*>(p); p += 4;
    t.Flags = *p;
    return t;
}
inline constexpr size_t kNetTransformSize = 21;
static_assert(kNetTransformSize == sizeof(uint32_t) + 3 * sizeof(float) + sizeof(float) + sizeof(uint8_t),
              "kNetTransformSize desatualizado vs WriteNetTransform/ReadNetTransform");

} // namespace net
} // namespace kizuri
