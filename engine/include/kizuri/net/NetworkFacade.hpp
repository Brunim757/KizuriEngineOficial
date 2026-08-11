#pragma once
#include "kizuri/net/Network.hpp"

namespace kizuri {

// Facade global de rede pro jogo (pilar AAA v0.34): uma sessão por processo,
// atualizada pelo Scene a cada frame (OnUpdateRuntimeLogic). Os eventos vão
// pra uma fila interna — o jogo (script) consulta via PollEvent. Host numa
// instância, clientes nas outras; porta local padrão do host: 26000.
class Network {
public:
    // Host da partida: escuta na porta (0 = 26000; use GetLocalPort()).
    static bool Host(uint16_t port = 26000);
    // Cliente: conecta num host.
    static bool Connect(const std::string& address, uint16_t port = 26000);
    static void Shutdown();
    static bool IsRunning() { return s_Session.IsRunning(); }
    static bool IsHost() { return s_Session.IsHost(); }
    static uint16_t GetLocalPort() { return s_Session.GetLocalPort(); }

    // Envia dados confiáveis pro peer (1..N; cliente usa 1 = o host).
    static bool Send(uint32_t peer, const void* data, size_t size);

    // Chamado pelo Scene todo frame (não chame manualmente no jogo).
    static void Update(float dt);

    // Próximo evento pendente; devolve false se a fila está vazia.
    static bool PollEvent(net::Event& out);

private:
    static net::NetworkSession s_Session;
    static std::vector<net::Event> s_Queue;
};

} // namespace kizuri
