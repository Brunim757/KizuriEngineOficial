#pragma once
#include <utility>

namespace kizuri {

// Códigos de tecla espelham GLFW para permitir cast direto, mas isolam o
// jogo/KZScript da dependência direta de GLFW.
namespace Key {
    enum : int {
        Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
        D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,
        Semicolon = 59, Equal = 61,
        A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Escape = 256, Enter, Tab, Backspace, Insert, Delete,
        Right, Left, Down, Up,
        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        LeftShift = 340, LeftControl, LeftAlt, LeftSuper,
        RightShift, RightControl, RightAlt, RightSuper
    };
}

namespace Mouse {
    enum : int { Button0 = 0, Button1, Button2, Left = Button0, Right = Button1, Middle = Button2 };
}

class Input {
public:
    // Não-inline de propósito — ver Input.cpp. Mesmo bug do
    // Application::s_Instance: "static inline" duplica a variável por
    // binário (DLL vs executável); com o corpo aqui, cada chamador
    // acabaria escrevendo/lendo a própria cópia isolada.
    static void SetContext(void* nativeWindow);

    static bool IsKeyPressed(int keycode);
    static bool IsMouseButtonPressed(int button);
    static std::pair<float, float> GetMousePosition();
    static float GetMouseX();
    static float GetMouseY();

private:
    static void* s_Window;
};

} // namespace kizuri
