#pragma once
#include <cstdint>
#include <functional>
#include <random>

namespace kizuri {






class UUID {
public:
    UUID() : m_UUID(Generate()) {}
    explicit UUID(uint64_t uuid) : m_UUID(uuid) {}
    UUID(const UUID&) = default;
    UUID& operator=(const UUID&) = default;

    operator uint64_t() const { return m_UUID; }

    bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
    bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }

    
    bool IsValid() const { return m_UUID != 0; }
    static UUID Invalid() { return UUID(uint64_t(0)); }

private:
    static uint64_t Generate() {
        static std::random_device rd;
        static std::mt19937_64 engine(rd());
        static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
        return dist(engine);
    }

    uint64_t m_UUID;
};

} 

namespace std {
template<>
struct hash<kizuri::UUID> {
    std::size_t operator()(const kizuri::UUID& uuid) const noexcept {
        return std::hash<uint64_t>()(static_cast<uint64_t>(uuid));
    }
};
} 
