#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace kizuri {
namespace net {













enum class EventType : uint8_t { Connect, Disconnect, Data, Error };

struct Event {
    EventType Type = EventType::Error;
    uint32_t Peer = 0;          
    std::vector<uint8_t> Data;
};


class NetSocket {
public:
    NetSocket() = default;
    ~NetSocket() { Close(); }
    NetSocket(const NetSocket&) = delete;
    NetSocket& operator=(const NetSocket&) = delete;

    bool Create();                      
    void Close();
    bool Bind(uint16_t port);           
    void SetNonBlocking(bool nb);       
    bool SendTo(const std::string& addr, uint16_t port, const void* data, size_t size);
    
    bool Receive(void* buf, size_t cap, size_t& outSize, std::string& outAddr, uint16_t& outPort);

private:
    uintptr_t m_Socket = 0; 
};



class NetworkSession {
public:
    ~NetworkSession() { Stop(); }

    
    bool Host(uint16_t port = 0);
    
    bool Connect(const std::string& address, uint16_t port);
    void Stop();

    bool IsRunning() const { return m_Running; }
    bool IsHost() const { return m_IsHost; }
    uint16_t GetLocalPort() const;

    
    
    bool Send(uint32_t peer, const void* data, size_t size);
    
    bool Broadcast(uint16_t port, const void* data, size_t size);

    
    
    void Update(float dt, const std::function<void(const Event&)>& handler);

private:
    struct Peer {
        std::string Addr;
        uint16_t Port = 0;
        uint32_t SeqIn = 0;      
        uint32_t SeqOut = 0;     
        uint32_t AckSent = 0;    
        uint32_t NextAckWait = 0;
        float RetransmitTimer = 0.0f;
        float Timeout = 0.0f;
        bool Connected = false;
        std::vector<uint8_t> Pending; 
    };

    void SendPacket(Peer& peer, uint8_t flags, const void* payload, size_t size, bool reliable);
    bool ParsePacket(const uint8_t* buf, size_t size, uint8_t& flags, uint32_t& seq, uint32_t& ack, const uint8_t*& payload, size_t& payloadSize);
    void HandlePacket(const uint8_t* buf, size_t size, const std::string& addr, uint16_t port,
                      const std::function<void(const Event&)>& handler);

    NetSocket m_Socket;
    bool m_Running = false;
    bool m_IsHost = false;
    uint16_t m_Port = 0;

    
    std::vector<Peer> m_Peers;
    uint32_t m_NextPeerId = 1;
    std::string m_ServerAddr;
    uint16_t m_ServerPort = 0;
    uint32_t m_ServerPeerId = 0;
};



struct NetTransform {
    uint32_t EntityId = 0;   
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    float Yaw = 0.0f;
    uint8_t Flags = 0;       
};





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

} 
} 
