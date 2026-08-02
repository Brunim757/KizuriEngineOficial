#include "kizuri/scripting/ScriptEngine.hpp"
#include "kizuri/core/Log.hpp"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace kizuri {

void* ScriptEngine::s_ModuleHandle = nullptr;
ScriptRegistry ScriptEngine::s_Registry;
std::string ScriptEngine::s_LastError;
std::string ScriptEngine::s_LoadedPath;

using RegisterScriptsFn = void(*)(ScriptRegistry&);

bool ScriptEngine::LoadModule(const std::string& path) {
    UnloadModule();

#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(path.c_str());
    if (!handle) {
        s_LastError = "Não foi possível carregar a biblioteca (arquivo não existe, não é um .dll válido, ou depende de outra DLL faltando).";
        KZ_CORE_ERROR("Não foi possível carregar o GameModule: {0}", path);
        return false;
    }
    auto registerFn = reinterpret_cast<RegisterScriptsFn>(GetProcAddress(handle, "RegisterScripts"));
    if (!registerFn) {
        s_LastError = "A biblioteca carregou, mas não exporta RegisterScripts (esqueceu extern \"C\"?).";
        KZ_CORE_ERROR("GameModule '{0}' não exporta RegisterScripts (esqueceu extern \"C\"?)", path);
        FreeLibrary(handle);
        return false;
    }
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        s_LastError = std::string("Não foi possível carregar a biblioteca: ") + dlerror();
        KZ_CORE_ERROR("Não foi possível carregar o GameModule: {0} ({1})", path, dlerror());
        return false;
    }
    auto registerFn = reinterpret_cast<RegisterScriptsFn>(dlsym(handle, "RegisterScripts"));
    if (!registerFn) {
        s_LastError = "A biblioteca carregou, mas não exporta RegisterScripts (esqueceu extern \"C\"?).";
        KZ_CORE_ERROR("GameModule '{0}' não exporta RegisterScripts (esqueceu extern \"C\"?)", path);
        dlclose(handle);
        return false;
    }
#endif

    s_ModuleHandle = handle;
    s_Registry.Clear();
    registerFn(s_Registry);

    s_LastError.clear();
    s_LoadedPath = path;
    KZ_CORE_INFO("GameModule carregado: {0} ({1} script(s) registrado(s))", path, s_Registry.GetClassNames().size());
    return true;
}

void ScriptEngine::UnloadModule() {
    s_LoadedPath.clear();
    if (!s_ModuleHandle) return;

    // Limpa o registro ANTES de descarregar a biblioteca — os factories
    // guardados em ScriptRegistry são ponteiros de função que vivem dentro
    // do módulo; chamá-los depois do dlclose/FreeLibrary seria acessar
    // memória já desmapeada.
    s_Registry.Clear();

#if defined(_WIN32)
    FreeLibrary((HMODULE)s_ModuleHandle);
#else
    dlclose(s_ModuleHandle);
#endif
    s_ModuleHandle = nullptr;
}

bool ScriptEngine::IsModuleLoaded() { return s_ModuleHandle != nullptr; }

const std::string& ScriptEngine::GetLastError() { return s_LastError; }
const std::string& ScriptEngine::GetLoadedPath() { return s_LoadedPath; }

ScriptRegistry& ScriptEngine::GetRegistry() { return s_Registry; }

} // namespace kizuri
