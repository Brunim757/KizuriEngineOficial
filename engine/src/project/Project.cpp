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

// Escreve um Source/ que já compila sozinho — não só a pasta vazia. Desde a
// v2 o jogo é um assembly .NET (C#): o csproj referencia a Kizuri.Scripting
// via a raiz do checkout da engine (mesma ideia do antigo KIZURI_ENGINE_DIR
// do CMake), e um script de exemplo já registrado aparece no Inspetor.
// OutputType Exe de propósito: só aplicações geram o .runtimeconfig.json/
// .deps.json que o host CoreCLR embutido (CoreCLRHost) usa pra inicializar.
static void WriteSourceTemplate(const fs::path& sourceDir) {
    std::ofstream csproj(sourceDir / "Game.csproj");
    csproj <<
        "<Project Sdk=\"Microsoft.NET.Sdk\">\n\n"
        "  <!--\n"
        "    Jogo em C# usando a API Kizuri.Scripting (Unity-like). Compile com:\n"
        "      dotnet build -p:EngineDir=/caminho/pra/KizuriEngineOficial\n"
"    O assembly resultante (bin/Debug/net10.0/Game.dll) é o que o editor\n"
         "    carrega em Arquivo > Carregar GameModule.\n"
         "  -->\n"
         "  <PropertyGroup>\n"
         "    <OutputType>Exe</OutputType>\n"
         "    <TargetFramework>net10.0</TargetFramework>\n"
        "    <Nullable>enable</Nullable>\n"
        "    <ImplicitUsings>enable</ImplicitUsings>\n"
        "    <AssemblyName>Game</AssemblyName>\n"
        "    <EngineDir Condition=\"'$(EngineDir)' == ''\">$(KIZURI_ENGINE_DIR)</EngineDir>\n"
        "  </PropertyGroup>\n\n"
        "  <ItemGroup>\n"
        "    <ProjectReference Include=\"$(EngineDir)/managed/Kizuri.Scripting/Kizuri.Scripting.csproj\" />\n"
        "  </ItemGroup>\n\n"
        "</Project>\n";
    csproj.close();

    // Main exigida pelo OutputType Exe (o que gera o runtimeconfig.json/
    // deps.json). Nenhuma lógica roda aqui — quem comanda é a engine.
    std::ofstream program(sourceDir / "Program.cs");
    program <<
        "// Main de aplicação exigida pelo OutputType Exe (o que gera o\n"
        "// Game.runtimeconfig.json/.deps.json que o host CoreCLR usa).\n"
        "// Quem comanda o jogo é a engine — os scripts são chamados via\n"
        "// [GameEntryPoint] e Script, não por este Main.\n"
        "internal static class Program\n"
        "{\n"
        "    private static void Main()\n"
        "    {\n"
        "    }\n"
        "}\n";
    program.close();

    std::ofstream example(sourceDir / "ExampleScript.cs");
    example <<
        "using Kizuri;\n"
        "using Kizuri.Math;\n\n"
        "// Script de exemplo — apague ou renomeie à vontade. Qualquer classe que\n"
        "// herdar de Script e for registrada no [GameEntryPoint] (ver abaixo)\n"
        "// aparece no dropdown \"Script Nativo\" do Inspetor assim que o assembly\n"
        "// for carregado (menu Arquivo > Carregar GameModule).\n"
        "public sealed class ExampleScript : Script\n"
        "{\n"
        "    public override void OnCreate()\n"
        "    {\n"
        "        Log.Info($\"ExampleScript na entidade '{Entity.Id}' pronta.\");\n"
        "    }\n\n"
        "    public override void OnUpdate(float deltaSeconds)\n"
        "    {\n"
        "        // Seu código de gameplay aqui. 'deltaSeconds' é o tempo do frame.\n"
        "        if (Entity.TryGetTransform(out var t))\n"
        "        {\n"
        "            t.Translation.X += 1.0f * deltaSeconds;\n"
        "            Entity.SetPosition(t.Translation);\n"
        "        }\n"
        "    }\n\n"
        "    public override void OnCollisionBegin(Entity other) { }\n"
        "    public override void OnCollisionEnd(Entity other) { }\n"
        "    public override void OnDestroy() { }\n"
        "}\n\n"
        "// Registro global dos scripts (equivalente 'RegisterScripts(name)').\n"
        "public static class MyGameModule\n"
        "{\n"
        "    [GameEntryPoint]\n"
        "    public static void RegisterAll()\n"
        "    {\n"
        "        GameModule.Register<ExampleScript>(\"ExampleScript\");\n"
        "    }\n"
        "}\n";
    example.close();
}

Ref<Project> Project::New(const std::string& directory, const std::string& name, ProjectMode mode) {
    Ref<Project> project = CreateRef<Project>();
    project->m_ProjectDirectory = directory;
    project->m_Config.Name = name.empty() ? "Novo Projeto" : name;
    project->m_Config.DefaultMode = mode;

    std::error_code ec;
    fs::create_directories(fs::path(directory) / project->m_Config.AssetDirectory, ec);
    if (ec) KZ_CORE_ERROR("Falha ao criar o diretório de assets do projeto: {0}", ec.message());

    // Source/ com um Game.csproj e um script de exemplo já funcionando — não
    // só a pasta vazia. É o que faz "o C# vem no projeto" ser uma frase
    // concreta: compila esse diretório sozinho (com -p:EngineDir apontando
    // pro checkout da engine) e já tem um Game.dll pronto pra carregar pelo
    // menu Arquivo > Carregar GameModule.
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
    cfg.GameModulePath = root.value("GameModulePath", "");

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
    root["GameModulePath"] = m_Config.GameModulePath;

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

std::string Project::MakeRelativePath(const std::string& path) {
    if (path.empty()) return path;
    if (path.rfind("builtin:", 0) == 0) return path;
    if (path.rfind("kzres://", 0) == 0) return path;

    auto& active = GetActive();
    if (!active) return path;

    std::error_code ec;
    fs::path absPath = fs::weakly_canonical(fs::absolute(path), ec);
    if (ec) absPath = fs::absolute(path);
    fs::path root = fs::weakly_canonical(fs::absolute(active->GetProjectDirectory()), ec);
    if (ec) root = fs::absolute(active->GetProjectDirectory());

    fs::path rel = fs::relative(absPath, root, ec);
    if (ec || rel.empty() || *rel.begin() == "..") return path;
    return rel.generic_string();
}

std::string Project::ResolvePath(const std::string& path) {
    if (path.empty()) return path;
    if (path.rfind("builtin:", 0) == 0) return path;
    if (path.rfind("kzres://", 0) == 0) return path; // recurso embutido, sem resolução de disco

    fs::path p(path);
    if (p.is_absolute()) return path;

    auto& active = GetActive();
    if (active) {
        fs::path candidate = fs::path(active->GetProjectDirectory()) / p;
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
    return path;
}

} // namespace kizuri
