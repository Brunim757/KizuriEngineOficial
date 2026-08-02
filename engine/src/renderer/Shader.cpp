#include "kizuri/renderer/Shader.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <vector>

namespace kizuri {

static uint32_t CompileStage(GLenum type, const std::string& source, const std::string& shaderName) {
    uint32_t shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int isCompiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        int len;
        glGetShaderiv(shader, 0x8B84 /*GL_INFO_LOG_LENGTH*/, &len);
        std::vector<char> info(len);
        glGetShaderInfoLog(shader, len, &len, info.data());
        KZ_CORE_ERROR("Falha ao compilar o shader '{0}': {1}", shaderName, info.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

Shader::Shader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
    : m_Name(name) {
    KZ_TRACE_SCOPE("Shader::Shader");
    uint32_t vs = CompileStage(GL_VERTEX_SHADER, vertexSrc, name);
    uint32_t fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc, name);

    m_RendererID = glCreateProgram();
    glAttachShader(m_RendererID, vs);
    glAttachShader(m_RendererID, fs);
    glLinkProgram(m_RendererID);

    int isLinked;
    glGetProgramiv(m_RendererID, GL_LINK_STATUS, &isLinked);
    if (!isLinked) {
        int len;
        glGetProgramiv(m_RendererID, 0x8B84, &len);
        std::vector<char> info(len);
        glGetProgramInfoLog(m_RendererID, len, &len, info.data());
        KZ_CORE_ERROR("Falha ao vincular o programa de shader '{0}': {1}", name, info.data());
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    KZ_CORE_INFO("Shader '{0}' compilado com sucesso (id={1}).", name, m_RendererID);
}

Shader::~Shader() { glDeleteProgram(m_RendererID); }

void Shader::Bind() const   { glUseProgram(m_RendererID); }
void Shader::Unbind() const { glUseProgram(0); }

int Shader::GetUniformLocation(const std::string& name) {
    auto it = m_UniformLocationCache.find(name);
    if (it != m_UniformLocationCache.end()) return it->second;
    int loc = glGetUniformLocation(m_RendererID, name.c_str());
    m_UniformLocationCache[name] = loc;
    return loc;
}

void Shader::SetInt(const std::string& name, int value)              { glUniform1i(GetUniformLocation(name), value); }
void Shader::SetIntArray(const std::string& name, int* values, uint32_t count) {
    int loc = GetUniformLocation(name);
    for (uint32_t i = 0; i < count; ++i) glUniform1i(loc + (int)i, values[i]);
}
void Shader::SetFloat(const std::string& name, float value)          { glUniform1f(GetUniformLocation(name), value); }
void Shader::SetFloat3(const std::string& name, const glm::vec3& v)  { glUniform3f(GetUniformLocation(name), v.x, v.y, v.z); }
void Shader::SetFloat4(const std::string& name, const glm::vec4& v)  { glUniform4f(GetUniformLocation(name), v.x, v.y, v.z, v.w); }
void Shader::SetMat3(const std::string& name, const glm::mat3& m)    { glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::SetMat4(const std::string& name, const glm::mat4& m)    { glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(m)); }

Ref<Shader> Shader::CreateFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    auto readFile = [](const std::string& path) {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    };
    return CreateRef<Shader>(vertexPath, readFile(vertexPath), readFile(fragmentPath));
}

// ---- ShaderLibrary ----
void ShaderLibrary::Add(const Ref<Shader>& shader) { m_Shaders[shader->GetName()] = shader; }

Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
    auto shader = CreateRef<Shader>(name, vertexSrc, fragmentSrc);
    Add(shader);
    return shader;
}

Ref<Shader> ShaderLibrary::Get(const std::string& name) { return m_Shaders.at(name); }
bool ShaderLibrary::Exists(const std::string& name) const { return m_Shaders.find(name) != m_Shaders.end(); }

} // namespace kizuri
