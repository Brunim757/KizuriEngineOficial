#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/Event.hpp"
#include <string>
#include <vector>

#if defined(KZ_PLATFORM_ANDROID)
struct ANativeWindow;
#endif
struct GLFWwindow;

namespace kizuri {

struct WindowProps {
    std::string Title = "Kizuri Engine";
    uint32_t Width = 1600;
    uint32_t Height = 900;
    bool VSync = true;
    bool Maximized = false;

    bool CustomTitlebar = false;
};

class Window {
public:
    explicit Window(const WindowProps& props = WindowProps());
    ~Window();

    void OnUpdate();
    void SetEventCallback(const EventCallbackFn& cb) { m_Data.EventCallback = cb; }

    uint32_t GetWidth() const  { return m_Data.Width; }
    uint32_t GetHeight() const { return m_Data.Height; }

    void SetVSync(bool enabled);
    bool IsVSync() const { return m_Data.VSync; }

    std::vector<std::string>& GetDroppedFiles() { return m_Data.DroppedFiles; }

    GLFWwindow* GetNativeWindow() const { return m_Window; }

    int GetGLVersionMajor() const { return m_GLVersionMajor; }
    int GetGLVersionMinor() const { return m_GLVersionMinor; }

    void GetPosition(int& x, int& y) const;
    void SetPosition(int x, int y);
    void SetSize(int width, int height);
    void Minimize();
    void ToggleMaximize();
    bool IsMaximized() const;

private:
    void Init(const WindowProps& props);
    void Shutdown();

#if defined(KZ_PLATFORM_ANDROID)
    void* m_EGLDisplay = nullptr;
    void* m_EGLSurface = nullptr;
    void* m_EGLContext = nullptr;
    void* m_EGLConfig = nullptr;
    bool m_SurfaceValid = false;
    void HandleAndroidSurfaceChanged(void* nativeWindow);
    void DestroyAndroidEGLSurface();
#endif
    GLFWwindow* m_Window = nullptr;
    int m_GLVersionMajor = 0;
    int m_GLVersionMinor = 0;

    struct WindowData {
        std::string Title;
        uint32_t Width = 0, Height = 0;
        bool VSync = true;
        EventCallbackFn EventCallback;

        std::vector<std::string> DroppedFiles;
    };
    WindowData m_Data;
};

}
