#pragma once
#include "kizuri/Core.hpp"
#include <string>

namespace kizuri {

// Define só os PADRÕES visuais que o editor sugere ao abrir o projeto
// (câmera inicial, grid inicial — ver docs/NOTAS_INTERNAS.md). Nunca uma
// restrição técnica: um projeto "TwoD" pode ganhar um MeshRendererComponent
// no dia seguinte sem nenhuma migração, porque a cena sempre foi híbrida
// por baixo.
enum class ProjectMode { TwoD, ThreeD, Empty };

struct ProjectConfig {
    std::string Name = "Novo Projeto";
    ProjectMode DefaultMode = ProjectMode::Empty;

    // Caminhos relativos à pasta do projeto (a que contém o .kzproj) — é
    // isso que torna um projeto portável: mover a pasta inteira pra outro
    // PC não quebra nenhuma referência.
    std::string AssetDirectory = "Assets";
    std::string StartScenePath; // vazio = nenhuma cena inicial definida ainda
    std::string GameModulePath; // relativo ao projeto ou absoluto — auto-carrega ao abrir

    // Build settings (usadas no exportar jogo / janela do executável).
    std::string GameName;        // vazio = usa o nome do projeto
    std::string Version = "1.0"; // versão exibida/gravada
    int WindowWidth = 1280;      // resolução da janela do jogo
    int WindowHeight = 720;
};

// Um projeto Kizuri é uma pasta com um arquivo .kzproj na raiz, uma pasta
// Assets/ com o conteúdo, e uma pasta Source/ com o código C# do jogo
// (um Game.csproj que referencia a Kizuri.Scripting — ver
// docs/NOTAS_INTERNAS.md). Esta classe só cuida do arquivo .kzproj e da
// resolução de caminho; não sabe nada sobre cena ou asset em si.
class Project {
public:
    // Cria a estrutura de pastas (Assets/, Source/) e o .kzproj num
    // diretório novo, e já marca esse projeto como o ativo.
    static Ref<Project> New(const std::string& directory, const std::string& name, ProjectMode mode);

    // Carrega um .kzproj existente e marca como ativo.
    static Ref<Project> Load(const std::string& kzprojFilePath);

    bool Save();

    const ProjectConfig& GetConfig() const { return m_Config; }
    ProjectConfig& GetConfig() { return m_Config; }

    const std::string& GetProjectDirectory() const { return m_ProjectDirectory; }
    const std::string& GetFilePath() const { return m_FilePath; }

    // Caminho absoluto pra pasta de assets — é o que qualquer código de
    // import/carregamento de asset deveria usar como raiz, em vez de
    // caminho relativo ao diretório de trabalho do processo (que muda
    // dependendo de como o editor foi lançado).
    std::string GetAssetDirectory() const;

    // Se o caminho estiver dentro do projeto ativo, devolve relativo à
    // pasta do .kzproj (ex: "Assets/hero.png"). Caso contrário, devolve
    // o original. Sem projeto aberto, devolve o original.
    static std::string MakeRelativePath(const std::string& path);

    // Resolve caminho relativo contra a pasta do projeto ativo (ou CWD).
    // Caminhos absolutos e "builtin:..." passam intactos.
    static std::string ResolvePath(const std::string& path);

    static Ref<Project>& GetActive();

private:
    ProjectConfig m_Config;
    std::string m_ProjectDirectory;
    std::string m_FilePath;
};

} // namespace kizuri
