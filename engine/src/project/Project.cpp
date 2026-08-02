#include "kizuri/project/Project.hpp"
#include "kizuri/core/Log.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace kizuri {

Ref<Project>& Project::GetActive() {
    static Ref<Project> s_ActiveProject;
    return s_ActiveProject;
}

static const char* ModeToString(ProjectMode mode) {
    switch (mode) {
        case ProjectMode::TwoD:   return "2D";
        case ProjectMode::ThreeD: return "3D";
        default:                 return "Empty";
    }
}

static ProjectMode ModeFromString(const std::string& s) {
    if (s == "2D") return ProjectMode::TwoD;
    if (s == "3D") return ProjectMode::ThreeD;
    return ProjectMode::Empty;
}

// Escreve um Source/ que já compila sozinho — não só a pasta vazia. Três
// arquivos: o CMakeLists.txt do GameModule, um header pequeno com a macro
// de export (__declspec(dllexport) no Windows — sem isso, o símbolo
// RegisterScripts não fica visível de fora da DLL e o ScriptEngine não
// acha ele via GetProcAddress), e um script de exemplo já registrado.
static void WriteSourceTemplate(const fs::path& sourceDir) {
    std::ofstream cmake(sourceDir / "CMakeLists.txt");
    cmake <<
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(GameModule CXX)\n\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "# Aponte pra pasta RAIZ do seu checkout da Kizuri Engine (o repositório\n"
        "# inteiro que tem engine/, editor/ etc — não só a pasta engine/ sozinha).\n"
        "# Ex: cmake -B build -DKIZURI_ENGINE_DIR=/caminho/pra/Kizuri-Engine-main\n"
        "if(NOT DEFINED KIZURI_ENGINE_DIR OR KIZURI_ENGINE_DIR STREQUAL \"\")\n"
        "    message(FATAL_ERROR \"Defina -DKIZURI_ENGINE_DIR=/caminho/pra/Kizuri-Engine-main ao configurar este projeto.\")\n"
        "endif()\n\n"
        "# add_subdirectory (não find_package): a engine ainda não tem um passo\n"
        "# de instalação/export — isso compila o motor junto, do jeito mais\n"
        "# simples que funciona hoje. Um find_package(Kizuri) de verdade é\n"
        "# trabalho futuro quando a engine ganhar um CMake install() completo.\n"
        "#\n"
        "# KizuriEngine é SHARED (não STATIC) exatamente pra este caso: quando\n"
        "# o editor já rodando carrega este GameModule em runtime, o sistema\n"
        "# operacional reconhece que uma biblioteca chamada \"KizuriEngine\"\n"
        "# já está carregada no processo e reaproveita ELA, em vez de carregar\n"
        "# uma segunda cópia — é o que garante log, estado de projeto e alocação\n"
        "# de memória compartilhados entre o editor e o seu script. Pra isso\n"
        "# funcionar direito, compile este projeto com o MESMO compilador que\n"
        "# compilou o editor (mesma versão do MSVC, ou do GCC/Clang) — nomes de\n"
        "# símbolo e layout de struct precisam bater.\n"
        "add_subdirectory(${KIZURI_ENGINE_DIR}/engine ${CMAKE_BINARY_DIR}/_kizuri_engine)\n\n"
        "file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS \"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp\")\n\n"
        "add_library(GameModule SHARED ${GAME_SOURCES})\n"
        "target_link_libraries(GameModule PRIVATE KizuriEngine)\n"
        "target_include_directories(GameModule PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})\n";
    cmake.close();

    std::ofstream apiHeader(sourceDir / "GameModuleAPI.hpp");
    apiHeader <<
        "#pragma once\n\n"
        "// __declspec(dllexport) no Windows: sem isso o símbolo RegisterScripts\n"
        "// fica interno à DLL e o ScriptEngine (dlopen/GetProcAddress) não\n"
        "// consegue achar ele pelo nome. No Linux/macOS não precisa de nada\n"
        "// especial, símbolos são visíveis por padrão.\n"
        "#if defined(_WIN32)\n"
        "    #define KZ_GAME_MODULE_API __declspec(dllexport)\n"
        "#else\n"
        "    #define KZ_GAME_MODULE_API\n"
        "#endif\n";
    apiHeader.close();

    std::ofstream exampleHpp(sourceDir / "ExampleScript.hpp");
    exampleHpp <<
        "#pragma once\n"
        "#include <Kizuri.hpp>\n\n"
        "// Script de exemplo — apague ou renomeie à vontade. Qualquer classe que\n"
        "// herdar de kizuri::NativeScript e for registrada em RegisterScripts()\n"
        "// (ver ExampleScript.cpp) aparece no dropdown \"Script Nativo\" do\n"
        "// Inspetor assim que este GameModule for carregado (menu Arquivo >\n"
        "// Carregar GameModule).\n"
        "class ExampleScript : public kizuri::NativeScript {\n"
        "public:\n"
        "    void OnCreate() override;\n"
        "    void OnUpdate(kizuri::Timestep ts) override;\n"
        "};\n";
    exampleHpp.close();

    std::ofstream exampleCpp(sourceDir / "ExampleScript.cpp");
    exampleCpp <<
        "#include \"ExampleScript.hpp\"\n"
        "#include \"GameModuleAPI.hpp\"\n\n"
        "void ExampleScript::OnCreate() {\n"
        "    KZ_TRACE(\"ExampleScript::OnCreate na entidade '{0}'\", GetEntity().GetName());\n"
        "}\n\n"
        "void ExampleScript::OnUpdate(kizuri::Timestep ts) {\n"
        "    // Seu código de gameplay aqui. 'ts' é o tempo do frame em segundos.\n"
        "    (void)ts;\n"
        "}\n\n"
        "// extern \"C\" evita name mangling do C++ — sem isso o nome exportado da\n"
        "// função viraria algo como \"?RegisterScripts@@YAXAEAV...\" em vez de\n"
        "// \"RegisterScripts\", e o dlsym/GetProcAddress por nome não acharia.\n"
        "extern \"C\" KZ_GAME_MODULE_API void RegisterScripts(kizuri::ScriptRegistry& registry) {\n"
        "    registry.Register<ExampleScript>(\"ExampleScript\");\n"
        "}\n";
    exampleCpp.close();
}

Ref<Project> Project::New(const std::string& directory, const std::string& name, ProjectMode mode) {
    Ref<Project> project = CreateRef<Project>();
    project->m_ProjectDirectory = directory;
    project->m_Config.Name = name.empty() ? "Novo Projeto" : name;
    project->m_Config.DefaultMode = mode;

    std::error_code ec;
    fs::create_directories(fs::path(directory) / project->m_Config.AssetDirectory, ec);
    if (ec) KZ_CORE_ERROR("Falha ao criar o diretório de assets do projeto: {0}", ec.message());

    // Source/ com um CMakeLists.txt e um script de exemplo já funcionando
    // — não só a pasta vazia. É o que faz "o C++ vem no projeto" ser uma
    // frase concreta: compila esse diretório sozinho (apontando
    // KIZURI_ENGINE_DIR pro checkout da engine) e já tem um GameModule.dll
    // pronto pra carregar pelo menu Arquivo > Carregar GameModule.
    fs::path sourceDir = fs::path(directory) / "Source";
    fs::create_directories(sourceDir, ec);
    if (!ec) WriteSourceTemplate(sourceDir);

    project->m_FilePath = (fs::path(directory) / (project->m_Config.Name + ".kzproj")).string();
    project->Save();

    GetActive() = project;
    return project;
}

Ref<Project> Project::Load(const std::string& kzprojFilePath) {
    std::ifstream in(kzprojFilePath);
    if (!in.is_open()) {
        KZ_CORE_ERROR("Não foi possível abrir o projeto: {0}", kzprojFilePath);
        return nullptr;
    }

    json root;
    in >> root;

    Ref<Project> project = CreateRef<Project>();
    project->m_FilePath = kzprojFilePath;
    project->m_ProjectDirectory = fs::path(kzprojFilePath).parent_path().string();

    auto& cfg = project->m_Config;
    cfg.Name = root.value("Name", "Projeto");
    cfg.DefaultMode = ModeFromString(root.value("DefaultMode", "Empty"));
    cfg.AssetDirectory = root.value("AssetDirectory", "Assets");
    cfg.StartScenePath = root.value("StartScenePath", "");

    KZ_CORE_INFO("Projeto carregado: {0}.", cfg.Name);
    GetActive() = project;
    return project;
}

bool Project::Save() {
    if (m_FilePath.empty()) return false;

    json root;
    root["Name"] = m_Config.Name;
    root["DefaultMode"] = ModeToString(m_Config.DefaultMode);
    root["AssetDirectory"] = m_Config.AssetDirectory;
    root["StartScenePath"] = m_Config.StartScenePath;

    std::ofstream out(m_FilePath);
    if (!out.is_open()) {
        KZ_CORE_ERROR("Não foi possível salvar o projeto em: {0}", m_FilePath);
        return false;
    }
    out << root.dump(4);
    return true;
}

std::string Project::GetAssetDirectory() const {
    return (fs::path(m_ProjectDirectory) / m_Config.AssetDirectory).string();
}

} // namespace kizuri
