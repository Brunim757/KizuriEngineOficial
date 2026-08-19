
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_activity.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "kizuri/core/Application.hpp"
#include "kizuri/core/CommandLineArgs.hpp"
#include "kizuri/core/AndroidPlatform.hpp"
#include "kizuri/core/Log.hpp"

namespace kizuri { Application* CreateApplication(); }

namespace kizuri {
namespace android {

static android_app* s_App = nullptr;
static Application* s_GameApp = nullptr;

static bool MkDirs(const std::string& path) {
    std::string cur;
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos && pos > 0) {
        cur = path.substr(0, pos);
        if (mkdir(cur.c_str(), 0777) != 0 && errno != EEXIST) return false;
    }
    return true;
}

static void ExtractDir(AAssetManager* mgr, const std::string& assetDir,
                       const std::string& destDir) {
    AAssetDir* dir = AAssetManager_openDir(mgr, assetDir.c_str());
    if (!dir) {
        KZ_CORE_WARN("Assets '{0}' não encontrados — pulando extração.", assetDir);
        return;
    }
    MkDirs(destDir);
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
        std::string src = (assetDir.empty() ? std::string() : assetDir + "/") + name;
        std::string dst = destDir + "/" + name;
        AAsset* asset = AAssetManager_open(mgr, src.c_str(), AASSET_MODE_STREAMING);
        if (!asset) { KZ_CORE_WARN("Falha ao abrir asset '{0}'.", src); continue; }
        FILE* out = fopen(dst.c_str(), "wb");
        if (!out) {
            KZ_CORE_ERROR("Falha ao criar '{0}' (extração).", dst);
            AAsset_close(asset);
            continue;
        }
        char buf[65536];
        int n = 0;
        long total = 0;
        while ((n = AAsset_read(asset, buf, sizeof(buf))) > 0) {
            (void)fwrite(buf, 1, (size_t)n, out);
            total += n;
        }
        fclose(out);
        AAsset_close(asset);
        KZ_CORE_INFO("Extraído: {0} ({1} bytes).", src, total);
    }
    AAssetDir_close(dir);
}

static std::string FindGameDll(const std::string& dotnetDir) {
    struct stat st;
    DIR* dir = opendir(dotnetDir.c_str());
    if (!dir) return {};
    std::string found;
    while (dirent* e = readdir(dir)) {
        std::string name = e->d_name;
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".dll") != 0) continue;
        std::string rc = dotnetDir + "/" + name.substr(0, name.size() - 4) + ".runtimeconfig.json";
        if (stat(rc.c_str(), &st) == 0) { found = dotnetDir + "/" + name; break; }
    }
    closedir(dir);
    if (found.empty()) {

        found = dotnetDir + "/SampleGame.dll";
    }
    return found;
}

static void ExtractAppAssets(AAssetManager* mgr, const std::string& filesDir) {
    const std::string dotnetDir = filesDir + "/dotnet";
    const std::string gameDir = filesDir + "/game";
    const std::string marker = filesDir + "/.kizuri_extracted";

    struct stat st;
    if (stat(marker.c_str(), &st) == 0) return;

    MkDirs(dotnetDir);
    MkDirs(gameDir);

    ExtractDir(mgr, "dotnet", dotnetDir);
    ExtractDir(mgr, "game", gameDir);

    chdir(filesDir.c_str());
    setenv("KIZURI_FILES_DIR", filesDir.c_str(), 1);

    FILE* f = fopen(marker.c_str(), "w");
    if (f) { fputs("1\n", f); fclose(f); }
    KZ_CORE_INFO("Extração dos assets concluída em {0}.", filesDir);
}

static void PumpGlue() {
    if (!s_App) return;

    int fd = 0, events = 0;
    android_poll_source* source = nullptr;
    while (ALooper_pollOnce(0, &fd, &events, (void**)&source) >= 0) {
        if (source) source->process(s_App, source);
        if (s_App->destroyRequested) return;
    }
}

static bool ShouldExit() {
    return s_App ? s_App->destroyRequested != 0 : false;
}

static void HandleAppCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {

            AndroidPlatform::SetNativeWindow(app->window);
            if (!s_GameApp) {

                auto& args = GetCommandLineArgs();
                if (args.empty()) {
                    const std::string& files = AndroidPlatform::GetFilesDir();
                    args.emplace_back(files + "/game/Start.kzscene");
                    args.emplace_back(FindGameDll(files + "/dotnet"));
                }
                s_GameApp = CreateApplication();
            }
            break;
        }
        case APP_CMD_TERM_WINDOW:

            AndroidPlatform::SetNativeWindow(nullptr);
            break;
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
            AndroidPlatform::HandleAppPause();
            break;
        case APP_CMD_RESUME:
        case APP_CMD_START:
            AndroidPlatform::HandleAppResume();
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
            break;
        default:
            break;
    }
}

static int32_t HandleInputEvent(android_app* app, AInputEvent* event) {
    (void)app;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int32_t action = AMotionEvent_getAction(event);
    int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    size_t count = AMotionEvent_getPointerCount(event);

    if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_CANCEL) {

        int id = AMotionEvent_getPointerId(event, 0);
        AndroidPlatform::HandleTouch(AMotionEvent_getX(event, 0),
                                     AMotionEvent_getY(event, 0), false, id);
        return 1;
    }

    if (masked == AMOTION_EVENT_ACTION_POINTER_UP) {
        int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                  >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        if (idx >= 0 && (size_t)idx < count) {
            int id = AMotionEvent_getPointerId(event, idx);
            AndroidPlatform::HandleTouch(AMotionEvent_getX(event, idx),
                                         AMotionEvent_getY(event, idx), false, id);
        }
        return 1;
    }

    for (size_t i = 0; i < count; ++i) {
        if (count > (size_t)AndroidPlatform::kMaxTouches) break;
        int id = AMotionEvent_getPointerId(event, i);
        bool down = false;
        if (masked == AMOTION_EVENT_ACTION_DOWN && i == 0) down = true;
        else if (masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                      >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            if ((int)i == idx) down = true;
        } else if (masked == AMOTION_EVENT_ACTION_MOVE) down = true;
        if (!down) continue;
        AndroidPlatform::HandleTouch(AMotionEvent_getX(event, i),
                                     AMotionEvent_getY(event, i), true, id);
    }
    return 1;
}

}
}

void android_main(android_app* app) {

    setenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "1", 1);
    setenv("DOTNET_EnableWriteXorExecute", "0", 1);

    setenv("DOTNET_LogLevel", "3", 0);

    kizuri::android::s_App = app;
    app->onAppCmd = kizuri::android::HandleAppCmd;
    app->onInputEvent = kizuri::android::HandleInputEvent;

    kizuri::Log::Init();
    KZ_CORE_INFO("Kizuri Engine Android: android_main iniciado.");

    kizuri::AndroidPlatform::SetFilesDir(app->activity->internalDataPath);
    kizuri::AndroidPlatform::SetExternalFilesDir(app->activity->externalDataPath);
    kizuri::android::ExtractAppAssets(app->activity->assetManager,
                                      app->activity->internalDataPath);

    kizuri::AndroidPlatform::SetGlueHooks(kizuri::android::PumpGlue,
                                          kizuri::android::ShouldExit);

    ANativeActivity_setWindowFlags(app->activity,
                                   (uint32_t)0x00000001, (uint32_t)0x00000001);

    while (!kizuri::android::s_GameApp && !app->destroyRequested) {
        int fd = 0, events = 0;
        android_poll_source* source = nullptr;
        ALooper_pollOnce(-1, &fd, &events, (void**)&source);
        if (source) source->process(app, source);
    }

    if (kizuri::android::s_GameApp) {
        kizuri::android::s_GameApp->Run();
    } else {
        KZ_CORE_ERROR("A engine não foi criada (sem APP_CMD_INIT_WINDOW?).");
    }
}