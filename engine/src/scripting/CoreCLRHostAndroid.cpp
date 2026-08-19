











#include "kizuri/scripting/CoreCLRHost.hpp"
#include "kizuri/scripting/dotnet/coreclr_delegates.h"
#include "kizuri/core/Log.hpp"

#include <filesystem>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstring>

#include <dlfcn.h>

namespace fs = std::filesystem;

namespace kizuri {
namespace scripting {

namespace {




static void* LoadLibraryNative(const std::string& path) {
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
}
static void FreeLibraryNative(void* handle) { dlclose(handle); }
static void* GetProcNative(void* handle, const char* name) { return dlsym(handle, name); }




using LoadAssemblyAndGetFunctionPointerFn =
    int (CORECLR_DELEGATE_CALLTYPE*)(const char* assemblyPath,
                                     const char* typeName,
                                     const char* methodName,
                                     const char* delegateTypeName,
                                     void* reserved,
                                     void** delegate);
using CoreclrInitializeFn =
    int32_t (*)(const char* exePath, const char* appDomainFriendlyName,
                int propertyCount, const char** propertyKeys,
                const char** propertyValues, void** hostHandle,
                unsigned int* domainId);
using CoreclrCreateDelegateFn =
    int32_t (*)(void* hostHandle, unsigned int domainId,
                const char* entryPointAssemblyName, const char* entryPointTypeName,
                const char* entryPointMethodName, void** delegate);
using CoreclrShutdownFn =
    int32_t (*)(void* hostHandle, unsigned int domainId);


using InitializeGameModuleFn = void (*)(const char* gameAssemblyPath);
using GetScriptCountFn = int (*)();
using GetScriptNameFn = int (*)(int index, char* buffer, int bufferSize);
using GetLastInitErrorFn = int (*)(char* buffer, int bufferSize);
using CreateScriptFn = void* (*)(const char* className, uint32_t entityHandle);
using DestroyScriptFn = void (*)(void* handle);
using UpdateScriptFn = void (*)(void* handle, float deltaSeconds);
using CollisionScriptFn = void (*)(void* handle, uint32_t otherHandle, int begin);

constexpr const char* kHostTypeName = "Kizuri.Hosting.Host, Kizuri.Scripting";

static bool FileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

} 




struct CoreCLRHost::Impl {
    void* CoreclrLib = nullptr;
    void* HostHandle = nullptr;
    unsigned int DomainId = 0;
    CoreclrShutdownFn CoreclrShutdown = nullptr;
    LoadAssemblyAndGetFunctionPointerFn LoadAssemblyAndGetFunctionPointer = nullptr;

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

    
    
    fs::path appBase = fs::path(runtimeConfigPath).parent_path();
    fs::path coreclrPath = appBase / "libcoreclr.so";
    if (!FileExists(coreclrPath)) {
        outError = "libcoreclr.so não encontrado em: " + appBase.string();
        return false;
    }

    std::unique_ptr<Impl> impl(new Impl());
    impl->CoreclrLib = LoadLibraryNative(coreclrPath.string());
    if (impl->CoreclrLib == nullptr) {
        outError = "Falha ao carregar libcoreclr.so (" + coreclrPath.string() + ").";
        return false;
    }

    auto initialize = reinterpret_cast<CoreclrInitializeFn>(
        GetProcNative(impl->CoreclrLib, "coreclr_initialize"));
    auto createDelegate = reinterpret_cast<CoreclrCreateDelegateFn>(
        GetProcNative(impl->CoreclrLib, "coreclr_create_delegate"));
    impl->CoreclrShutdown = reinterpret_cast<CoreclrShutdownFn>(
        GetProcNative(impl->CoreclrLib, "coreclr_shutdown"));
    if (initialize == nullptr || createDelegate == nullptr || impl->CoreclrShutdown == nullptr) {
        outError = "libcoreclr.so não exporta coreclr_initialize/coreclr_create_delegate.";
        FreeLibraryNative(impl->CoreclrLib);
        return false;
    }

    
    
    
    std::string tpa;
    {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(appBase, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension() == ".dll") {
                if (!tpa.empty()) tpa += ':';
                tpa += entry.path().string();
            }
        }
    }
    if (tpa.empty()) {
        outError = "Nenhuma dll encontrada em: " + appBase.string();
        FreeLibraryNative(impl->CoreclrLib);
        return false;
    }

    std::string appPaths = appBase.string();
    std::string baseDir = appBase.string();
    const char* propertyKeys[] = {
        "TRUSTED_PLATFORM_ASSEMBLIES",
        "APP_PATHS",
        "APP_CONTEXT_BASE_DIRECTORY",
    };
    const char* propertyValues[] = { tpa.c_str(), appPaths.c_str(), baseDir.c_str() };

    int rc = initialize(assemblyPath.c_str(), "Kizuri.Domain", 3,
                        propertyKeys, propertyValues,
                        &impl->HostHandle, &impl->DomainId);
    if (rc != 0) {
        outError = "coreclr_initialize falhou (código " + std::to_string(rc) + ").";
        FreeLibraryNative(impl->CoreclrLib);
        return false;
    }

    rc = createDelegate(impl->HostHandle, impl->DomainId,
                        "System.Private.CoreLib",
                        "Internal.Runtime.InteropServices.ComponentActivator",
                        "LoadAssemblyAndGetFunctionPointer",
                        reinterpret_cast<void**>(&impl->LoadAssemblyAndGetFunctionPointer));
    if (rc != 0 || impl->LoadAssemblyAndGetFunctionPointer == nullptr) {
        outError = "coreclr_create_delegate falhou (código " + std::to_string(rc) + ").";
        impl->CoreclrShutdown(impl->HostHandle, impl->DomainId);
        impl->HostHandle = nullptr;
        FreeLibraryNative(impl->CoreclrLib);
        return false;
    }

    
    
    auto bind = [&](const char* method, const char* delegateType, void** out) -> bool {
        return impl->LoadAssemblyAndGetFunctionPointer(
            assemblyPath.c_str(), kHostTypeName, method, delegateType, nullptr, out) == 0;
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
        outError = "Falha ao obter os pontos de entrada do Kizuri.Scripting.";
        impl->CoreclrShutdown(impl->HostHandle, impl->DomainId);
        impl->HostHandle = nullptr;
        FreeLibraryNative(impl->CoreclrLib);
        return false;
    }

    
    impl->InitializeGameModule(assemblyPath.c_str());

    s_Impl = impl.release();
    return true;
}

void CoreCLRHost::Shutdown() {
    if (s_Impl == nullptr) return;
    if (s_Impl->CoreclrShutdown != nullptr && s_Impl->HostHandle != nullptr)
        s_Impl->CoreclrShutdown(s_Impl->HostHandle, s_Impl->DomainId);
    if (s_Impl->CoreclrLib != nullptr)
        FreeLibraryNative(s_Impl->CoreclrLib);
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

} 
} 