#include "kizuri/core/Input.hpp"
#include <GLFW/glfw3.h>
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
    auto* window = static_cast<GLFWwindow*>(s_Window);
    if (!window) return false;
    int state = glfwGetKey(window, keycode);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsMouseButtonPressed(int button) {
    auto* window = static_cast<GLFWwindow*>(s_Window);
    if (!window) return false;
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

std::pair<float, float> Input::GetMousePosition() {
    auto* window = static_cast<GLFWwindow*>(s_Window);
    if (!window) return { 0.0f, 0.0f };
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return { (float)x, (float)y };
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
