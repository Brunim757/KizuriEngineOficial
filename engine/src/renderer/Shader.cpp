#include "kizuri/renderer/Shader.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace kizuri {

// Versão GLSL do CONTEXTO atual (parse de GL_SHADING_LANGUAGE_VERSION uma
// vez, cacheado), TRAVADA no teto da versão que a janela criou de fato
// (SetContextGLSLVersion) — alguns drivers reportam GLSL maior que o
// contexto real (ex.: 4.60 num contexto 3.3) e compilar shader acima da GL
// quebra ou renderiza errado.
static int s_ContextGLSL = 0; // teto definido pela janela (0 = sem teto)
void SetContextGLSLVersion(int glsl) { s_ContextGLSL = glsl; }

static std::string s_RenderDiagnostic; // última falha de driver/shader (tela)
void SetShaderDiagnostic(const std::string& msg) { s_RenderDiagnostic = msg; }
const std::string& GetShaderDiagnostic() { return s_RenderDiagnostic; }

int GetGLSLVersion() {
    static int s_glsl = 0;
    if (s_glsl == 0) {
        const char* v = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        int major = 3, minor = 30;
        if (v) {
            const char* p = v;
            while (*p && !(*p >= '0' && *p <= '9')) ++p;
            if (*p >= '0' && *p <= '9') major = *p - '0';
            while (*p && *p != '.') ++p;
            if (*p == '.') {
                ++p;
                int m = 0;
                while (*p >= '0' && *p <= '9') { m = m * 10 + (*p - '0'); ++p; }
                minor = m;
            }
        }
        int parsed = major * 100 + minor;
        if (parsed < 330) parsed = 330;
        s_glsl = (s_ContextGLSL > 0) ? std::min(parsed, s_ContextGLSL) : parsed;
        KZ_CORE_INFO("Contexto GLSL {0} core.", s_glsl);
    }
    return s_glsl;
}

std::string GetOpenGLVersionString() {
    const char* v = (const char*)glGetString(GL_VERSION);
    return v ? std::string(v) : std::string("desconhecido");
}

namespace {

// Troca a linha "#version ..." (se houver) pela #version 330 core fixa.
// Remove a diretiva de versão de QUALQUER lugar (e pré-adiciona a 330),
// garantindo que o resultado sempre começa com "#version".
std::string RewriteVersionFor(const std::string& src, int glsl) {
    std::string body = src;
    size_t pos = body.find("#version");
    if (pos != std::string::npos) {
        size_t lineEnd = body.find('\n', pos);
        if (lineEnd != std::string::npos) body.erase(pos, lineEnd - pos + 1);
        else body.erase(pos);
    }
    // A engine roda SEMPRE em GLSL 330 core (não há mais escalonamento por
    // versão — as features 4.x foram removidas do código).
    return "#version 330 core\n" + body;
}

} // namespace

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

    // A engine roda SEMPRE em OpenGL 3.3 core — os shaders são #version 330
    // (injetado via RewriteVersionFor) e não existe mais escalonamento por
    // versão (as features 4.x foram removidas).
    uint32_t vs = CompileStage(GL_VERTEX_SHADER, RewriteVersionFor(vertexSrc, 330), name);
    uint32_t fs = CompileStage(GL_FRAGMENT_SHADER, RewriteVersionFor(fragmentSrc, 330), name);

    m_RendererID = glCreateProgram();
    if (vs != 0) glAttachShader(m_RendererID, vs);
    if (fs != 0) glAttachShader(m_RendererID, fs);
    glLinkProgram(m_RendererID);

    int isLinked;
    glGetProgramiv(m_RendererID, GL_LINK_STATUS, &isLinked);
    m_IsValid = (isLinked == GL_TRUE);
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);

    if (!m_IsValid) {
        int len;
        glGetProgramiv(m_RendererID, 0x8B84, &len);
        std::vector<char> info(len);
        glGetProgramInfoLog(m_RendererID, len, &len, info.data());
        std::string msg(info.data(), (size_t)std::max(len, 0));
        KZ_CORE_ERROR("Falha ao vincular o programa de shader '{0}': {1}", name, msg);
        SetShaderDiagnostic("SHADER '" + m_Name + "' FALHOU: " + msg);
        return;
    }
    KZ_CORE_INFO("Shader '{0}' compilado (GLSL 330, id={1}).", name, m_RendererID);
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
void Shader::SetFloat2(const std::string& name, const glm::vec2& v)  { glUniform2f(GetUniformLocation(name), v.x, v.y); }
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
