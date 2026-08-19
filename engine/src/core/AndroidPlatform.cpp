#include "kizuri/core/AndroidPlatform.hpp"
#include <android/native_window.h>
#include <cstring>
#include <cstdio>

namespace kizuri {
namespace AndroidPlatform {

namespace {

struct RawEvent {
    uint32_t Type = 0;
    int A = 0;     
    int B = 0;     
    float X = 0.0f;
    float Y = 0.0f;
};

ANativeWindow* s_NativeWindow = nullptr;
std::string s_FilesDir;
std::string s_ExternalFilesDir;

SurfaceChangedFn s_SurfaceChanged = nullptr;
void* s_SurfaceChangedUserData = nullptr;
GluePumpFn s_GluePump = nullptr;
ShouldExitFn s_ShouldExit = nullptr;

RawEvent s_EventQueue[128];
unsigned s_EventHead = 0; 
unsigned s_EventTail = 0; 

TouchPoint s_Touches[kMaxTouches];
int s_TouchCount = 0;
float s_LastTouchX = 0.0f;
float s_LastTouchY = 0.0f;

bool s_VirtualKeys[kVirtualKeyBase + 256] = {}; 

EventHandler s_Handler = nullptr;
void* s_HandlerUserData = nullptr;

void PushEvent(uint32_t type, int a = 0, int b = 0, float x = 0.0f, float y = 0.0f) {
    unsigned next = (s_EventHead + 1) % (sizeof(s_EventQueue) / sizeof(s_EventQueue[0]));
    if (next == s_EventTail) return; 
    RawEvent& ev = s_EventQueue[s_EventHead];
    ev.Type = type; ev.A = a; ev.B = b; ev.X = x; ev.Y = y;
    s_EventHead = next;
}

} 

void SetNativeWindow(ANativeWindow* window) {
    s_NativeWindow = window;
    if (s_SurfaceChanged) s_SurfaceChanged(window, s_SurfaceChangedUserData);
}
ANativeWindow* GetNativeWindow() { return s_NativeWindow; }

void SetSurfaceChangedCallback(SurfaceChangedFn fn, void* userData) {
    s_SurfaceChanged = fn;
    s_SurfaceChangedUserData = userData;
}

void SetGlueHooks(GluePumpFn pump, ShouldExitFn shouldExit) {
    s_GluePump = pump;
    s_ShouldExit = shouldExit;
}
void PumpGlue() { if (s_GluePump) s_GluePump(); }
bool ShouldExit() { return s_ShouldExit ? s_ShouldExit() : false; }

void SetFilesDir(const std::string& path) { s_FilesDir = path; }
const std::string& GetFilesDir() { return s_FilesDir; }
void SetExternalFilesDir(const std::string& path) { s_ExternalFilesDir = path; }
const std::string& GetExternalFilesDir() { return s_ExternalFilesDir; }

void HandleResize(int width, int height) { PushEvent(EvWindowResize, width, height); }
void HandleKey(int key, bool down, bool repeat) {
    PushEvent(down ? EvKeyPressed : EvKeyReleased, key, repeat ? 1 : 0);
}
void HandleMouseMove(float x, float y) {
    s_LastTouchX = x; s_LastTouchY = y;
    PushEvent(EvMouseMoved, 0, 0, x, y);
}
void HandleTouch(float x, float y, bool down, int id) {
    
    if (down) {
        bool exists = false;
        int slot = -1;
        for (int i = 0; i < kMaxTouches; ++i) {
            if (s_Touches[i].Id == id) { exists = true; slot = i; break; }
            if (slot < 0 && !s_Touches[i].Down) slot = i;
        }
        if (exists) {
            s_Touches[slot].X = x; s_Touches[slot].Y = y;
        } else if (slot >= 0) {
            s_Touches[slot] = TouchPoint{ id, x, y, true };
            if (s_TouchCount < kMaxTouches) s_TouchCount++;
        }
        s_LastTouchX = x; s_LastTouchY = y;
        SetVirtualKey(kVirtualKeyBase + id, true);
    } else {
        for (int i = 0; i < kMaxTouches; ++i) {
            if (s_Touches[i].Id == id && s_Touches[i].Down) {
                s_Touches[i].Down = false;
                s_Touches[i].Id = -1;
                s_TouchCount = 0; 
                break;
            }
        }
        s_TouchCount = 0;
        for (int i = 0; i < kMaxTouches; ++i)
            if (s_Touches[i].Down) s_TouchCount++;
        SetVirtualKey(kVirtualKeyBase + id, false);
    }
    PushEvent(down ? EvMouseButtonPressed : EvMouseButtonReleased, 0 , 0, x, y);
}
void HandleAppPause() { PushEvent(EvKeyReleased, 10000 , 0, 0, 0); }
void HandleAppResume() {}

void SetVirtualKey(int key, bool down) {
    if (key < 0 || key >= (int)(sizeof(s_VirtualKeys) / sizeof(s_VirtualKeys[0]))) return;
    s_VirtualKeys[key] = down;
}
bool IsVirtualKeyDown(int key) {
    if (key < 0 || key >= (int)(sizeof(s_VirtualKeys) / sizeof(s_VirtualKeys[0]))) return false;
    return s_VirtualKeys[key];
}

int GetTouchCount() { return s_TouchCount; }
const TouchPoint& GetTouch(int index) {
    static TouchPoint none;
    if (index < 0 || index >= kMaxTouches) return none;
    return s_Touches[index];
}
bool IsAnyTouchDown() {
    for (int i = 0; i < kMaxTouches; ++i)
        if (s_Touches[i].Down) return true;
    return false;
}
float GetLastTouchX() { return s_LastTouchX; }
float GetLastTouchY() { return s_LastTouchY; }

void PollEvents() {
    while (s_EventTail != s_EventHead) {
        const RawEvent& ev = s_EventQueue[s_EventTail];
        s_EventTail = (s_EventTail + 1) % (sizeof(s_EventQueue) / sizeof(s_EventQueue[0]));
        if (s_Handler)
            s_Handler(s_HandlerUserData, ev.Type, ev.A, ev.B, ev.X, ev.Y);
    }
}

void SetEventHandler(EventHandler handler, void* userData) {
    s_Handler = handler;
    s_HandlerUserData = userData;
}

} 
} 