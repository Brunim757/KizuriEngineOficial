#pragma once








#include <cstdint>
#include <string>

struct ANativeWindow;

namespace kizuri {
namespace AndroidPlatform {


constexpr int kMaxTouches = 4;

constexpr int kVirtualKeyBase = 10000;

struct TouchPoint {
    int Id = -1;
    float X = 0.0f;   
    float Y = 0.0f;
    bool Down = false;
};


void SetNativeWindow(ANativeWindow* window);
ANativeWindow* GetNativeWindow();


using SurfaceChangedFn = void (*)(void* nativeWindow, void* userData);
void SetSurfaceChangedCallback(SurfaceChangedFn fn, void* userData);



using GluePumpFn = void (*)();
using ShouldExitFn = bool (*)();
void SetGlueHooks(GluePumpFn pump, ShouldExitFn shouldExit);
void PumpGlue();
bool ShouldExit();


void SetFilesDir(const std::string& path);
const std::string& GetFilesDir();
void SetExternalFilesDir(const std::string& path);
const std::string& GetExternalFilesDir();


void HandleResize(int width, int height);
void HandleTouch(float x, float y, bool down, int id);
void HandleMouseMove(float x, float y);
void HandleKey(int key, bool down, bool repeat);
void HandleAppPause();
void HandleAppResume();


void SetVirtualKey(int key, bool down);
bool IsVirtualKeyDown(int key);


int GetTouchCount();
const TouchPoint& GetTouch(int index);
bool IsAnyTouchDown();
float GetLastTouchX();
float GetLastTouchY();



void PollEvents();


using EventHandler = void (*)(void* userData, uint32_t type, int keyCode, int action,
                              float x, float y);
enum : uint32_t {
    EvWindowResize = 1,
    EvKeyPressed,
    EvKeyReleased,
    EvMouseButtonPressed,
    EvMouseButtonReleased,
    EvMouseMoved,
};
void SetEventHandler(EventHandler handler, void* userData);

} 
} 