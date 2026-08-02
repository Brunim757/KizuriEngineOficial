#include "kizuri/ecs/Components.hpp"
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scripting/ScriptEngine.hpp"
#include "kizuri/core/Log.hpp"

namespace kizuri {

void NativeScriptComponent::BindByName(const std::string& className) {
    ClassName = className;

    // Captura só o nome (string), não um ponteiro de fábrica direto — se
    // o GameModule for recarregado entre o Bind e a instanciação de fato
    // (Play), essa busca por nome sempre pega a versão mais recente
    // registrada, em vez de um factory apontando pro módulo antigo.
    InstantiateScript = [className]() -> NativeScript* {
        NativeScript* instance = ScriptEngine::GetRegistry().Create(className);
        if (!instance)
            KZ_CORE_ERROR("Script '{0}' não encontrado no GameModule carregado.", className);
        return instance;
    };
    DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
}

} // namespace kizuri
