#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace kizuri {

// AssetManager evita recarregar o mesmo arquivo do disco várias vezes:
// mantém um cache indexado por caminho para texturas e meshes.
class AssetManager {
public:
    // Callback executada na MAIN THREAD quando a textura estiver pronta
    // (a GL texture só pode ser criada na thread com contexto).
    using TextureCallback = std::function<void(Ref<Texture2D>)>;

    static Ref<Texture2D> GetTexture(const std::string& path);
    static Ref<Mesh> GetMesh(const std::string& path);
    static void Clear();

    // Streaming (pilar AAA v0.34): decodifica a imagem numa thread de
    // trabalho e devolve a textura pronta via callback (main thread).
    // Já carregada em cache → o callback roda IMEDIATAMENTE (síncrono).
    static bool LoadTextureAsync(const std::string& path, TextureCallback callback);

    // Processa os loads terminados: cria a GL texture e chama o callback.
    // Chamado pela Application todo frame.
    static void TickAsyncLoads();

    // Espera as threads terminarem (fim do programa/Teste de shutdown).
    static void Shutdown();

private:
    // Sem 'inline' de propósito — mesmo bug do Application::s_Instance
    // (ver Application.cpp). GetTexture/GetMesh/Clear já eram funções de
    // verdade (não inline), então isso já funcionava certo na prática,
    // mas o "static inline" aqui ainda duplicava a variável por binário
    // à toa — corrigido por consistência com o resto da engine.
    static std::unordered_map<std::string, Ref<Texture2D>> s_Textures;
    static std::unordered_map<std::string, Ref<Mesh>> s_Meshes;
};

} // namespace kizuri
