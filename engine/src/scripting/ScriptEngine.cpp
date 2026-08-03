#include "kizuri/scripting/ScriptEngine.hpp"
#include "kizuri/scripting/CoreCLRHost.hpp"
#include "kizuri/scripting/ManagedScript.hpp"
#include "kizuri/core/Log.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace kizuri {

ScriptRegistry ScriptEngine::s_Registry;
std::string ScriptEngine::s_LastError;
std::string ScriptEngine::s_LoadedPath;

bool ScriptEngine::LoadModule(const std::string& path) {
    UnloadModule();

    fs::path assemblyPath(path);
    if (!fs::is_regular_file(assemblyPath)) {
        s_LastError = "Arquivo não encontrado: " + path;
        KZ_CORE_ERROR("Módulo do jogo não encontrado: {0}", path);
        return false;
    }

    // O .runtimeconfig.json do jogo fica do lado do assembly (saída do
    // `dotnet build`/`publish`). É ele que diz qual shared framework usar.
    fs::path runtimeConfig = assemblyPath.parent_path() /
                             (assemblyPath.stem().string() + ".runtimeconfig.json");

    std::string hostError;
    if (!scripting::CoreCLRHost::Initialize(runtimeConfig.string(), assemblyPath.string(), hostError)) {
        s_LastError = hostError;
        KZ_CORE_ERROR("Falha ao iniciar o runtime C#: {0}", hostError);
        return false;
    }

    s_Registry.Clear();
    int count = scripting::CoreCLRHost::GetScriptCount();
    if (count <= 0) {
        // O assembly subiu mas o [GameEntryPoint] não registrou nada — antes
        // isso fechava o modal "com sucesso" e o dropdown ficava vazio sem
        // nenhum erro. Agora reporta o erro real do lado managed.
        std::string initError = scripting::CoreCLRHost::GetLastInitError();
        s_LastError = "Assembly carregado mas nenhum script foi registrado.";
        if (!initError.empty()) s_LastError += " Erro do host: " + initError;
        scripting::CoreCLRHost::Shutdown();
        s_LoadedPath.clear();
        KZ_CORE_ERROR("Falha ao carregar o módulo do jogo: {0}", s_LastError);
        return false;
    }
    for (int i = 0; i < count; ++i) {
        std::string className = scripting::CoreCLRHost::GetScriptName(i);
        // Factory por nome, igual ao módulo C++ antigo — o editor lista e a
        // Scene instancia sem nunca conhecer o tipo managed.
        s_Registry.Register(className, [className]() -> NativeScript* {
            return new ManagedScript(className);
        });
    }

    s_LoadedPath = path;
    s_LastError.clear();
    KZ_CORE_INFO("Assembly do jogo carregado: {0} ({1} scripts registrados).", path, count);
    return true;
}

void ScriptEngine::UnloadModule() {
    if (!scripting::CoreCLRHost::IsInitialized()) return;
    // Limpa o registro antes de derrubar o runtime — os factories guardam
    // lambdas que criam ManagedScript (código nativo, seguro), mas os
    // GCHandles dos scripts vivos morreriam junto com o runtime.
    s_Registry.Clear();
    scripting::CoreCLRHost::Shutdown();
    s_LoadedPath.clear();
}

bool ScriptEngine::IsModuleLoaded() { return scripting::CoreCLRHost::IsInitialized(); }

const std::string& ScriptEngine::GetLastError() { return s_LastError; }
const std::string& ScriptEngine::GetLoadedPath() { return s_LoadedPath; }

ScriptRegistry& ScriptEngine::GetRegistry() { return s_Registry; }

} // namespace kizuri
