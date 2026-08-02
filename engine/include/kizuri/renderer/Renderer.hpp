#pragma once
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/renderer/Shader.hpp"

namespace kizuri {

class Renderer {
public:
    static void Init();
    static void Shutdown();
    static void OnWindowResize(uint32_t width, uint32_t height);

    // Não-inline de propósito — mesmo bug do Application::s_Instance (ver
    // Application.cpp): "static inline" duplicava s_ShaderLibrary por
    // binário, então um shader registrado do lado da DLL nunca apareceria
    // pra quem chamasse isso do lado do executável.
    static ShaderLibrary& GetShaderLibrary();

private:
    static ShaderLibrary s_ShaderLibrary;
};

} // namespace kizuri
