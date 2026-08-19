#pragma once
#include "kizuri/Core.hpp"
#include <string>

namespace kizuri {

enum class ProjectMode { TwoD, ThreeD, Empty };

struct ProjectConfig {
    std::string Name = "Novo Projeto";
    ProjectMode DefaultMode = ProjectMode::Empty;

    std::string AssetDirectory = "Assets";
    std::string StartScenePath;
    std::string GameModulePath;

    std::string GameName;
    std::string Version = "1.0";
    int WindowWidth = 1280;
    int WindowHeight = 720;
};

class Project {
public:

    static Ref<Project> New(const std::string& directory, const std::string& name, ProjectMode mode);

    static Ref<Project> Load(const std::string& kzprojFilePath);

    bool Save();

    const ProjectConfig& GetConfig() const { return m_Config; }
    ProjectConfig& GetConfig() { return m_Config; }

    const std::string& GetProjectDirectory() const { return m_ProjectDirectory; }
    const std::string& GetFilePath() const { return m_FilePath; }

    std::string GetAssetDirectory() const;

    static std::string MakeRelativePath(const std::string& path);

    static std::string ResolvePath(const std::string& path);

    static Ref<Project>& GetActive();

private:
    ProjectConfig m_Config;
    std::string m_ProjectDirectory;
    std::string m_FilePath;
};

}
