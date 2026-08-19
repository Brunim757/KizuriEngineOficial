#pragma once
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/renderer/Shader.hpp"

namespace kizuri {

class Renderer {
public:
    static void Init();
    static void Shutdown();
    static void OnWindowResize(uint32_t width, uint32_t height);

    static ShaderLibrary& GetShaderLibrary();

private:
    static ShaderLibrary s_ShaderLibrary;
};

}
