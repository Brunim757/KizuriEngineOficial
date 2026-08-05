#pragma once
#include "kizuri/Core.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace kizuri {

// Versão GLSL core do contexto atual (330, 400, 410, 430, 450, ...) —
// detectada do GL_VERSION em runtime. É o que faz os shaders escalarem:
// PC 3.3 usa 330, PC 4.5 usa 450.
// Versão GLSL core do contexto atual (330, 400, 410, 430, 450, ...) —
// detectada do runtime. É o que faz os shaders escalarem: PC 3.3 usa 330,
// PC 4.5 usa 450.
int GetGLSLVersion();

// Define a versão GLSL do CONTEXTO criado pela janela (usada como teto:
// alguns drivers reportam GL_SHADING_LANGUAGE_VERSION maior que o contexto
// real — ex.: 4.60 num contexto 3.3 — e compilar shader acima da GL real
// quebra). Chamado pelo Window logo após criar o contexto.
void SetContextGLSLVersion(int glsl);

std::string GetOpenGLVersionString();

class Shader {
public:
    Shader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    void Bind() const;
    void Unbind() const;

    void SetInt(const std::string& name, int value);
    void SetIntArray(const std::string& name, int* values, uint32_t count);
    void SetFloat(const std::string& name, float value);
    void SetFloat2(const std::string& name, const glm::vec2& value);
    void SetFloat3(const std::string& name, const glm::vec3& value);
    void SetFloat4(const std::string& name, const glm::vec4& value);
    void SetMat3(const std::string& name, const glm::mat3& value);
    void SetMat4(const std::string& name, const glm::mat4& value);

    const std::string& GetName() const { return m_Name; }

    static Ref<Shader> CreateFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

private:
    int GetUniformLocation(const std::string& name);

    uint32_t m_RendererID = 0;
    std::string m_Name;
    std::unordered_map<std::string, int> m_UniformLocationCache;
};

// Biblioteca simples de shaders indexada por nome, usada pelo Renderer.
class ShaderLibrary {
public:
    void Add(const Ref<Shader>& shader);
    Ref<Shader> Load(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
    Ref<Shader> Get(const std::string& name);
    bool Exists(const std::string& name) const;

private:
    std::unordered_map<std::string, Ref<Shader>> m_Shaders;
};

} // namespace kizuri
