#pragma once
// CSharpBridgeInternal.h — cabeçalho INTERNO da engine (não faz parte da
// API pública, não vai pro SDK). Usado por ManagedScript para registrar um
// handle opaco (uint32) para uma entidade qualquer no MESMO mapa do
// CSharpBridge, de forma que o C# possa referenciar a entidade via os
// 'kz_*' do ABI. O C# nunca vê este arquivo.
#include "kizuri/ecs/Entity.hpp"
#include <cstdint>

namespace kizuri {
namespace scripting {

// Retorna o handle (ou cria um novo) para 'entity' no mapa do CSharpBridge.
// Retorna 0 para entidade inválida.
uint32_t RegisterEntityHandle(Entity entity);

} // namespace scripting
} // namespace kizuri
