#pragma once
#include "kizuri/Core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace kizuri {

constexpr int kMaxSkinJoints = 64;

struct AnimChannel {
    enum class Path { Translation = 0, Rotation, Scale };
    int Joint = 0;
    Path Type = Path::Translation;
    std::vector<float> Times;
    std::vector<glm::vec4> Values;
};

struct AnimationClip {
    std::string Name;
    float Duration = 0.0f;
    std::vector<AnimChannel> Channels;
};

struct SkinJoint {
    int Parent = -1;
    std::string Name;
    glm::mat4 InverseBind = glm::mat4(1.0f);

    glm::vec3 T = glm::vec3(0.0f);
    glm::quat R = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 S = glm::vec3(1.0f);
};

struct SkinData {
    std::vector<SkinJoint> Joints;
    std::vector<AnimationClip> Clips;
    std::vector<int> Order;

    static Ref<SkinData> CreateFromGLTF(const std::string& path);
    static Ref<SkinData> CreateFromGLTFMemory(const void* data, std::size_t size);

    int GetClipIndex(const std::string& name) const;
    float GetClipDuration(const std::string& name) const;

    bool Evaluate(const std::string& clipName, float time, glm::mat4* outMatrices, int maxJoints) const;

    bool EvaluateGlobal(const std::string& clipName, float time, glm::mat4* outMatrices, int maxJoints) const;
};

}
