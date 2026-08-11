#pragma once
#include "kizuri/Core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace kizuri {

// Limite de juntas por skin (cobre personagens até ~64 ossos; o mínimo
// garantido de uniforms de vértice no GL 3.3 é 256 vec4 = 64 mat4 exatos).
constexpr int kMaxSkinJoints = 64;

// Um canal de animação: uma trilha TRS de UMA junta (todas as keyframes).
struct AnimChannel {
    enum class Path { Translation = 0, Rotation, Scale };
    int Joint = 0;
    Path Type = Path::Translation;
    std::vector<float> Times;         // segundos
    std::vector<glm::vec4> Values;    // vec3 (T/S) ou quat x,y,z,w (R)
};

// Uma animação completa do glTF: nome + duração + canais por junta.
struct AnimationClip {
    std::string Name;
    float Duration = 0.0f;
    std::vector<AnimChannel> Channels;
};

// Junta da hierarquia do esqueleto (uma por osso da skin).
struct SkinJoint {
    int Parent = -1;          // índice na lista de juntas (-1 = raiz)
    std::string Name;
    glm::mat4 InverseBind = glm::mat4(1.0f);
    // Pose de repouso local (TRS default do nó no arquivo) — o que a junta
    // faz quando nenhum canal a anima.
    glm::vec3 T = glm::vec3(0.0f);
    glm::quat R = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 S = glm::vec3(1.0f);
};

// Skin + animações parseadas de um .glb/.gltf (via cgltf). Parse único e
// reutilizado por todas as entidades que apontam pro mesmo arquivo. A
// avaliação de pose é CPU, por frame, no número de juntas da skin.
struct SkinData {
    std::vector<SkinJoint> Joints;
    std::vector<AnimationClip> Clips;
    std::vector<int> Order;   // ordem topológica (pai antes do filho)

    static Ref<SkinData> CreateFromGLTF(const std::string& path);
    static Ref<SkinData> CreateFromGLTFMemory(const void* data, std::size_t size); // .glb em memória (embutido)

    int GetClipIndex(const std::string& name) const;
    float GetClipDuration(const std::string& name) const;

    // Preenche outMatrices[0..count) com (global * inverseBind) da junta no
    // tempo 'time' do clip 'clipName'. clipName vazio ou inválido = pose de
    // repouso (bind pose). Devolve false se a skin não tem juntas.
    bool Evaluate(const std::string& clipName, float time, glm::mat4* outMatrices, int maxJoints) const;

    // Igual ao Evaluate, porém devolve as matrizes GLOBAIS (sem inverseBind)
    // — o que o blend de animação e o IK usam pra misturar/corrigir a pose.
    bool EvaluateGlobal(const std::string& clipName, float time, glm::mat4* outMatrices, int maxJoints) const;
};

} // namespace kizuri
