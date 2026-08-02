#include "kizuri/core/Input.hpp"
#include <GLFW/glfw3.h>

namespace kizuri {

void* Input::s_Window = nullptr;

void Input::SetContext(void* nativeWindow) { s_Window = nativeWindow; }

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

} // namespace kizuri
