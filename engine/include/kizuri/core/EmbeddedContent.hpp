#pragma once
#include <cstddef>
#include <string>

namespace kizuri {

// Buffer binário de um recurso embutido no executável (kzres://).
struct EmbeddedBuffer {
    const void* Data = nullptr;
    std::size_t Size = 0;
};

// Registro de recursos embutidos (gerado no build a partir de
// engine/resources/embedded_content/, via cmake/EmbedContent.cmake).
// Os carregadores da engine aceitam caminhos "kzres://<nome>" e resolvem
// aqui — o recurso vive dentro do .exe/.dll, sem arquivo solto no disco.
bool GetEmbeddedResource(const std::string& name, EmbeddedBuffer& out);
bool HasEmbeddedResource(const std::string& name);

// true se o caminho usa o esquema embutido ("kzres://...").
inline bool IsEmbeddedPath(const std::string& path) {
    return path.rfind("kzres://", 0) == 0;
}
// Extrai o nome do recurso de um caminho kzres:// ("kzres://a/b.glb" -> "a/b.glb").
inline std::string EmbeddedNameFromPath(const std::string& path) {
    return path.substr(8);
}

} // namespace kizuri
