#include "kizuri/core/Input.hpp"
#if defined(KZ_PLATFORM_ANDROID)
    #include "kizuri/core/AndroidPlatform.hpp"
#else
    #include <GLFW/glfw3.h>
#endif
#include <unordered_map>

namespace kizuri {

void* Input::s_Window = nullptr;

// Input Actions: nome da ação -> código de tecla. O jogo pode rebindar em
// runtime (SetActionKey) e persistir do jeito que quiser.
static std::unordered_map<std::string, int> s_Actions;

void Input::SetContext(void* nativeWindow) {
    s_Window = nativeWindow;
    // Ações padrão de gameplay (rebindáveis em runtime via SetActionKey).
    s_Actions.try_emplace("Pular", Key::Space);
    s_Actions.try_emplace("Esquerda", Key::A);
    s_Actions.try_emplace("Direita", Key::D);
    s_Actions.try_emplace("Cima", Key::W);
    s_Actions.try_emplace("Baixo", Key::S);
    s_Actions.try_emplace("Acao", Key::E);
    s_Actions.try_emplace("Correr", Key::LeftShift);
    s_Actions.try_emplace("Cancelar", Key::Escape);
}

bool Input::IsKeyPressed(int keycode) {
#if defined(KZ_PLATFORM_ANDROID)
    // Android não tem teclado físico: só teclas virtuais (zonas de toque
    // mapeadas pelo jogo via SetActionKey + AndroidPlatform::SetVirtualKey).
    return AndroidPlatform::IsVirtualKeyDown(keycode);
#else
    auto* window = static_cast<GLFWwindow*>(s_Window);
    if (!window) return false;
    int state = glfwGetKey(window, keycode);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
#endif
}

bool Input::IsMouseButtonPressed(int button) {
#if defined(KZ_PLATFORM_ANDROID)
    // Tocar na tela == clique de mouse (qualquer dedo). Botão 0 = esquerdo.
    if (button != 0) return false;
    return AndroidPlatform::IsAnyTouchDown();
#else
    auto* window = static_cast<GLFWwindow*>(s_Window);
    if (!window) return false;
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
#endif
}

std::pair<float, float> Input::GetMousePosition() {
#if defined(KZ_PLATFORM_ANDROID)
    return { AndroidPlatform::GetLastTouchX(), AndroidPlatform::GetLastTouchY() };
#else
    auto* window = static_cast<GLFWwindow*>(s_Window);
    if (!window) return { 0.0f, 0.0f };
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return { (float)x, (float)y };
#endif
}

float Input::GetMouseX() { return GetMousePosition().first; }
float Input::GetMouseY() { return GetMousePosition().second; }

bool Input::IsActionPressed(const std::string& action) {
    auto it = s_Actions.find(action);
    if (it == s_Actions.end()) return false;
    return IsKeyPressed(it->second);
}
void Input::SetActionKey(const std::string& action, int keycode) {
    s_Actions[action] = keycode;
}
int Input::GetActionKey(const std::string& action) {
    auto it = s_Actions.find(action);
    if (it == s_Actions.end()) return -1;
    return it->second;
}

} // namespace kizuri
