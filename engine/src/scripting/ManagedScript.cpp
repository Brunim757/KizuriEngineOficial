#include "kizuri/scripting/ManagedScript.hpp"
#include "kizuri/scripting/CoreCLRHost.hpp"
#include "kizuri/scripting/CSharpBridgeInternal.h"
#include "kizuri/core/Log.hpp"

namespace kizuri {

ManagedScript::~ManagedScript() {
    if (m_ManagedHandle) {
        scripting::CoreCLRHost::DestroyScript(m_ManagedHandle);
        m_ManagedHandle = nullptr;
    }
}

void ManagedScript::OnCreate() {
    uint32_t entityHandle = scripting::RegisterEntityHandle(GetEntity());
    m_ManagedHandle = scripting::CoreCLRHost::CreateScript(m_ClassName, entityHandle);
    if (m_ManagedHandle == nullptr)
        KZ_CORE_ERROR("Falha ao instanciar o script C# '{0}' (classe não registrada no GameModule?).", m_ClassName);
}

void ManagedScript::OnDestroy() {
    if (m_ManagedHandle) {
        scripting::CoreCLRHost::DestroyScript(m_ManagedHandle);
        m_ManagedHandle = nullptr;
    }
}

void ManagedScript::OnUpdate(Timestep ts) {
    if (m_ManagedHandle)
        scripting::CoreCLRHost::UpdateScript(m_ManagedHandle, ts.GetSeconds());
}

void ManagedScript::OnCollisionBegin(Entity other) {
    if (m_ManagedHandle)
        scripting::CoreCLRHost::CollisionScript(m_ManagedHandle, scripting::RegisterEntityHandle(other), true);
}

void ManagedScript::OnCollisionEnd(Entity other) {
    if (m_ManagedHandle)
        scripting::CoreCLRHost::CollisionScript(m_ManagedHandle, scripting::RegisterEntityHandle(other), false);
}

} // namespace kizuri
