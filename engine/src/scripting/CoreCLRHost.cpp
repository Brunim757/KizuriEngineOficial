// CoreCLRHost.cpp — host embutido do runtime .NET (CoreCLR) via hostfxr.
// Carrega o hostfxr dinamicamente (LoadLibrary/dlopen), inicializa o runtime
// com o .runtimeconfig.json do jogo, resolve os pontos de entrada managed
// (Kizuri.Hosting.Host via load_assembly_and_get_function_pointer) e expõe
// o ciclo de vida dos scripts C# para o resto da engine.
#include "kizuri/scripting/CoreCLRHost.hpp"
#include "kizuri/scripting/dotnet/hostfxr.h"
#include "kizuri/scripting/dotnet/coreclr_delegates.h"
#include "kizuri/core/Log.hpp"

#include <filesystem>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace kizuri {
namespace scripting {

namespace {

// ---------------------------------------------------------------------------
// Carregamento de biblioteca dinâmica (hostfxr) por plataforma.
// ---------------------------------------------------------------------------
#if defined(_WIN32)
    using NativeChar = wchar_t;

    static void* LoadLibraryNative(const std::wstring& path) {
        return static_cast<void*>(LoadLibraryW(path.c_str()));
    }
    static void FreeLibraryNative(void* handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
    }
    static void* GetProcNative(void* handle, const char* name) {
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
    }
    static std::wstring ToNative(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(static_cast<size_t>(len), L'\0');
        if (len > 0)
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
        return out;
    }
    static std::string ToUtf8(const std::wstring& s) {
        if (s.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(len), '\0');
        if (len > 0)
            WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, nullptr, nullptr);
        return out;
    }
    // Usa native() (wchar_t) direto — .string() converteria pro ACP do
    // Windows primeiro, estragando caminhos com acento (ex: João).
    static std::wstring ToNativePath(const fs::path& p) { return p.native(); }
#else
    using NativeChar = char;

    static void* LoadLibraryNative(const std::string& path) {
        return dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    }
    static void FreeLibraryNative(void* handle) { dlclose(handle); }
    static void* GetProcNative(void* handle, const char* name) { return dlsym(handle, name); }
    static std::string ToNative(const std::string& s) { return s; }
    static std::string ToUtf8(const std::string& s) { return s; }
    static std::string ToNativePath(const fs::path& p) { return p.string(); }
#endif

// ---------------------------------------------------------------------------
// Tipos das funções do hostfxr (espelham hostfxr.h / coreclr_delegates.h).
// ---------------------------------------------------------------------------
using LoadAssemblyAndGetFunctionPointerFn =
    int (CORECLR_DELEGATE_CALLTYPE*)(const NativeChar* assemblyPath,
                                     const NativeChar* typeName,
                                     const NativeChar* methodName,
                                     const NativeChar* delegateTypeName,
                                     void* reserved,
                                     void** delegate);
using HostfxrInitializeRuntimeConfigFn =
    int32_t (HOSTFXR_CALLTYPE*)(const NativeChar* runtimeConfigPath,
                                const hostfxr_initialize_parameters* parameters,
                                hostfxr_handle* hostContextHandle);
using HostfxrGetRuntimeDelegateFn =
    int32_t (HOSTFXR_CALLTYPE*)(hostfxr_handle hostContextHandle,
                                hostfxr_delegate_type type,
                                void** delegate);
using HostfxrCloseFn = int32_t (HOSTFXR_CALLTYPE*)(hostfxr_handle hostContextHandle);
using HostfxrSetErrorWriterFn =
    hostfxr_error_writer_fn (HOSTFXR_CALLTYPE*)(hostfxr_error_writer_fn errorWriter);

// Assinaturas dos pontos de entrada managed (espelham Hosting/Host.cs).
using InitializeGameModuleFn = void (*)(const char* gameAssemblyPath);
using GetScriptCountFn = int (*)();
using GetScriptNameFn = int (*)(int index, char* buffer, int bufferSize);
using GetLastInitErrorFn = int (*)(char* buffer, int bufferSize);
using CreateScriptFn = void* (*)(const char* className, uint32_t entityHandle);
using DestroyScriptFn = void (*)(void* handle);
using UpdateScriptFn = void (*)(void* handle, float deltaSeconds);
using CollisionScriptFn = void (*)(void* handle, uint32_t otherHandle, int begin);

constexpr const char* kHostTypeName = "Kizuri.Hosting.Host, Kizuri.Scripting";

std::string s_HostfxrError;

void HostfxrErrorWriter(const NativeChar* message) {
    s_HostfxrError = ToUtf8(message);
}

// ---------------------------------------------------------------------------
// Descoberta do hostfxr (auto-contido primeiro, depois instalações do .NET).
// ---------------------------------------------------------------------------
static bool FileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

static bool IsNewerVersion(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& s) {
        std::vector<int> out;
        std::string cur;
        for (char c : s) {
            if (c == '.') { out.push_back(std::atoi(cur.c_str())); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
        return out;
    };
    auto pa = parts(a);
    auto pb = parts(b);
    for (size_t i = 0; i < pa.size() || i < pb.size(); ++i) {
        int x = i < pa.size() ? pa[i] : 0;
        int y = i < pb.size() ? pb[i] : 0;
        if (x != y) return x > y;
    }
    return false;
}

// Procura <fxrRoot>/<versão>/<lib> e devolve o path da maior versão.
static fs::path FindHostfxrInFxrRoot(const fs::path& fxrRoot) {
    std::error_code ec;
    if (!fs::is_directory(fxrRoot, ec)) return {};
    fs::path best;
    std::string bestVersion;
    for (auto& entry : fs::directory_iterator(fxrRoot, ec)) {
        if (!entry.is_directory(ec)) continue;
        std::string version = entry.path().filename().string();
#if defined(_WIN32)
        fs::path candidate = entry.path() / L"hostfxr.dll";
#else
        fs::path candidate = entry.path() / "libhostfxr.so";
#endif
        if (!FileExists(candidate)) continue;
        if (best.empty() || IsNewerVersion(version, bestVersion)) {
            best = candidate;
            bestVersion = version;
        }
    }
    return best;
}

static fs::path FindHostfxr(const fs::path& appBase) {
    // 1. Auto-contido: hostfxr do lado do assembly do jogo.
#if defined(_WIN32)
    fs::path local = appBase / L"hostfxr.dll";
#else
    fs::path local = appBase / "libhostfxr.so";
#endif
    if (FileExists(local)) return local;

    // 2. DOTNET_ROOT/host/fxr/<versão>.
    if (const char* root = std::getenv("DOTNET_ROOT")) {
        fs::path p = FindHostfxrInFxrRoot(fs::path(root) / "host" / "fxr");
        if (!p.empty()) return p;
    }

    // 3. Caminhos padrão de instalação.
    std::vector<fs::path> installRoots;
#if defined(_WIN32)
    installRoots.emplace_back("C:\\Program Files\\dotnet");
    installRoots.emplace_back("C:\\Program Files (x86)\\dotnet");
#else
    installRoots.emplace_back("/usr/share/dotnet");
    installRoots.emplace_back("/usr/local/share/dotnet");
    if (const char* home = std::getenv("HOME"))
        installRoots.emplace_back(fs::path(home) / ".dotnet");
#endif
    for (const fs::path& root : installRoots) {
        fs::path p = FindHostfxrInFxrRoot(root / "host" / "fxr");
        if (!p.empty()) return p;
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// Implementação (esconde os tipos do hosting .NET do header público).
// ---------------------------------------------------------------------------
struct CoreCLRHost::Impl {
    void* HostfxrLib = nullptr;
    hostfxr_handle Context = nullptr;
    LoadAssemblyAndGetFunctionPointerFn LoadAssemblyAndGetFunctionPointer = nullptr;
    HostfxrCloseFn Close = nullptr;
    HostfxrSetErrorWriterFn SetErrorWriter = nullptr;

    InitializeGameModuleFn InitializeGameModule = nullptr;
    GetScriptCountFn GetScriptCount = nullptr;
    GetScriptNameFn GetScriptName = nullptr;
    GetLastInitErrorFn GetLastInitError = nullptr;
    CreateScriptFn CreateScript = nullptr;
    DestroyScriptFn DestroyScript = nullptr;
    UpdateScriptFn UpdateScript = nullptr;
    CollisionScriptFn CollisionScript = nullptr;
};

CoreCLRHost::Impl* CoreCLRHost::s_Impl = nullptr;

bool CoreCLRHost::Initialize(const std::string& runtimeConfigPath,
                             const std::string& assemblyPath,
                             std::string& outError) {
    if (s_Impl != nullptr) {
        outError = "Runtime .NET já inicializado.";
        return false;
    }

    fs::path rcPath(runtimeConfigPath);
    if (!FileExists(rcPath)) {
        outError = "runtimeconfig.json não encontrado: " + runtimeConfigPath;
        return false;
    }

    fs::path hostfxrPath = FindHostfxr(rcPath.parent_path());
    if (hostfxrPath.empty()) {
        outError = "hostfxr não encontrado. Instale o .NET Runtime 8 ou publique o jogo como self-contained.";
        return false;
    }

    std::unique_ptr<Impl> impl(new Impl());
    impl->HostfxrLib = LoadLibraryNative(ToNativePath(hostfxrPath));
    if (impl->HostfxrLib == nullptr) {
        outError = "Falha ao carregar o hostfxr: " + hostfxrPath.string();
        return false;
    }

    auto init = reinterpret_cast<HostfxrInitializeRuntimeConfigFn>(
        GetProcNative(impl->HostfxrLib, "hostfxr_initialize_for_runtime_config"));
    auto getDelegate = reinterpret_cast<HostfxrGetRuntimeDelegateFn>(
        GetProcNative(impl->HostfxrLib, "hostfxr_get_runtime_delegate"));
    impl->Close = reinterpret_cast<HostfxrCloseFn>(
        GetProcNative(impl->HostfxrLib, "hostfxr_close"));
    impl->SetErrorWriter = reinterpret_cast<HostfxrSetErrorWriterFn>(
        GetProcNative(impl->HostfxrLib, "hostfxr_set_error_writer"));

    if (init == nullptr || getDelegate == nullptr || impl->Close == nullptr) {
        outError = "O hostfxr carregado não exporta as funções esperadas.";
        FreeLibraryNative(impl->HostfxrLib);
        return false;
    }

    if (impl->SetErrorWriter != nullptr) {
        s_HostfxrError.clear();
        impl->SetErrorWriter(HostfxrErrorWriter);
    }

    // Raiz do .NET da qual o hostfxr veio (para o initialize resolver o
    // shared framework). Auto-contido = a própria pasta do jogo.
    fs::path dotnetRoot = hostfxrPath.parent_path().parent_path().parent_path().parent_path();
    if (hostfxrPath.parent_path() == rcPath.parent_path())
        dotnetRoot = hostfxrPath.parent_path();

    hostfxr_initialize_parameters params = {};
    params.size = sizeof(params);
    auto dotnetRootNative = ToNativePath(dotnetRoot);
    params.dotnet_root = dotnetRootNative.c_str();

    auto rcPathNative = ToNativePath(rcPath);
    int rc = init(rcPathNative.c_str(), &params, &impl->Context);
    if (rc != 0) {
        outError = "hostfxr_initialize_for_runtime_config falhou (código " +
                   std::to_string(rc) + "). " + s_HostfxrError;
        FreeLibraryNative(impl->HostfxrLib);
        return false;
    }

    rc = getDelegate(impl->Context, hdt_load_assembly_and_get_function_pointer,
                     reinterpret_cast<void**>(&impl->LoadAssemblyAndGetFunctionPointer));
    if (rc != 0 || impl->LoadAssemblyAndGetFunctionPointer == nullptr) {
        outError = "hostfxr_get_runtime_delegate falhou (código " +
                   std::to_string(rc) + "). " + s_HostfxrError;
        impl->Close(impl->Context);
        FreeLibraryNative(impl->HostfxrLib);
        return false;
    }

    // Resolve os pontos de entrada managed (Kizuri.Hosting.Host no assembly
    // do jogo; a Kizuri.Scripting.dll é resolvida via deps.json no mesmo
    // contexto — por isso usamos o caminho do assembly do jogo).
    auto bind = [&](const char* method, const char* delegateType, void** out) -> bool {
        auto asmNative = ToNative(assemblyPath);
        auto typeNative = ToNative(kHostTypeName);
        auto methodNative = ToNative(method);
        auto delegateNative = ToNative(delegateType);
        return impl->LoadAssemblyAndGetFunctionPointer(
            asmNative.c_str(), typeNative.c_str(), methodNative.c_str(),
            delegateNative.c_str(), nullptr, out) == 0;
    };

    bool ok = true;
    ok = ok && bind("InitializeGameModule", "Kizuri.Hosting.InitializeGameModuleFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->InitializeGameModule));
    ok = ok && bind("GetScriptCount", "Kizuri.Hosting.GetScriptCountFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->GetScriptCount));
    ok = ok && bind("GetScriptName", "Kizuri.Hosting.GetScriptNameFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->GetScriptName));
    ok = ok && bind("GetLastInitError", "Kizuri.Hosting.GetLastInitErrorFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->GetLastInitError));
    ok = ok && bind("CreateScript", "Kizuri.Hosting.CreateScriptFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->CreateScript));
    ok = ok && bind("DestroyScript", "Kizuri.Hosting.DestroyScriptFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->DestroyScript));
    ok = ok && bind("UpdateScript", "Kizuri.Hosting.UpdateScriptFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->UpdateScript));
    ok = ok && bind("CollisionScript", "Kizuri.Hosting.CollisionScriptFn, Kizuri.Scripting",
                    reinterpret_cast<void**>(&impl->CollisionScript));
    if (!ok) {
        outError = "Falha ao obter os pontos de entrada do Kizuri.Scripting. " + s_HostfxrError;
        impl->Close(impl->Context);
        FreeLibraryNative(impl->HostfxrLib);
        return false;
    }

    // Dispara os [GameEntryPoint] do jogo — eles registram os scripts
    // (GameModule.Register) que o editor/runtime vão listar e instanciar.
    s_HostfxrError.clear();
    impl->InitializeGameModule(assemblyPath.c_str());

    s_Impl = impl.release();
    return true;
}

void CoreCLRHost::Shutdown() {
    if (s_Impl == nullptr) return;
    if (s_Impl->SetErrorWriter != nullptr)
        s_Impl->SetErrorWriter(nullptr);
    if (s_Impl->Close != nullptr && s_Impl->Context != nullptr)
        s_Impl->Close(s_Impl->Context);
    if (s_Impl->HostfxrLib != nullptr)
        FreeLibraryNative(s_Impl->HostfxrLib);
    delete s_Impl;
    s_Impl = nullptr;
}

bool CoreCLRHost::IsInitialized() { return s_Impl != nullptr; }

int CoreCLRHost::GetScriptCount() {
    if (s_Impl == nullptr || s_Impl->GetScriptCount == nullptr) return 0;
    return s_Impl->GetScriptCount();
}

std::string CoreCLRHost::GetScriptName(int index) {
    if (s_Impl == nullptr || s_Impl->GetScriptName == nullptr) return {};
    int length = s_Impl->GetScriptName(index, nullptr, 0);
    if (length <= 0) return {};
    std::string buffer(static_cast<size_t>(length) + 1, '\0');
    s_Impl->GetScriptName(index, &buffer[0], static_cast<int>(buffer.size()));
    buffer.resize(static_cast<size_t>(length));
    return buffer;
}

std::string CoreCLRHost::GetLastInitError() {
    if (s_Impl == nullptr || s_Impl->GetLastInitError == nullptr) return {};
    int length = s_Impl->GetLastInitError(nullptr, 0);
    if (length <= 0) return {};
    std::string buffer(static_cast<size_t>(length) + 1, '\0');
    s_Impl->GetLastInitError(&buffer[0], static_cast<int>(buffer.size()));
    buffer.resize(static_cast<size_t>(length));
    return buffer;
}

void* CoreCLRHost::CreateScript(const std::string& className, uint32_t entityHandle) {
    if (s_Impl == nullptr || s_Impl->CreateScript == nullptr) return nullptr;
    return s_Impl->CreateScript(className.c_str(), entityHandle);
}

void CoreCLRHost::DestroyScript(void* handle) {
    if (s_Impl == nullptr || s_Impl->DestroyScript == nullptr) return;
    s_Impl->DestroyScript(handle);
}

void CoreCLRHost::UpdateScript(void* handle, float deltaSeconds) {
    if (s_Impl == nullptr || s_Impl->UpdateScript == nullptr) return;
    s_Impl->UpdateScript(handle, deltaSeconds);
}

void CoreCLRHost::CollisionScript(void* handle, uint32_t otherHandle, bool begin) {
    if (s_Impl == nullptr || s_Impl->CollisionScript == nullptr) return;
    s_Impl->CollisionScript(handle, otherHandle, begin ? 1 : 0);
}

} // namespace scripting
} // namespace kizuri
