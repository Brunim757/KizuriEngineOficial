#include "kizuri/ecs/Animator.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/EmbeddedContent.hpp"
#include <cgltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace kizuri {

namespace {

glm::vec4 SampleChannel(const AnimChannel& ch, float time) {
    if (ch.Times.size() == 1) return ch.Values[0];
    size_t hi = 0;
    while (hi + 1 < ch.Times.size() && ch.Times[hi + 1] < time) ++hi;
    if (hi + 1 >= ch.Times.size()) return ch.Values[hi];
    float t0 = ch.Times[hi], t1 = ch.Times[hi + 1];
    float f = (t1 - t0) > 0.0001f ? (time - t0) / (t1 - t0) : 0.0f;
    if (ch.Type == AnimChannel::Path::Rotation) {
        glm::quat a(ch.Values[hi].w, ch.Values[hi].x, ch.Values[hi].y, ch.Values[hi].z);
        glm::quat b(ch.Values[hi + 1].w, ch.Values[hi + 1].x, ch.Values[hi + 1].y, ch.Values[hi + 1].z);
        glm::quat r = glm::slerp(a, b, f);
        return { r.x, r.y, r.z, r.w };
    }
    return glm::mix(ch.Values[hi], ch.Values[hi + 1], f);
}

}

static Ref<SkinData> BuildSkinFromGLTF(cgltf_data* data, const std::string& label);

Ref<SkinData> SkinData::CreateFromGLTF(const std::string& path) {
    if (IsEmbeddedPath(path)) {
        EmbeddedBuffer buf;
        if (!GetEmbeddedResource(EmbeddedNameFromPath(path), buf)) {
            KZ_CORE_ERROR("Animator: recurso embutido não encontrado '{0}'.", path);
            return nullptr;
        }
        return CreateFromGLTFMemory(buf.Data, buf.Size);
    }

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Animator: falha ao parsear '{0}' (cgltf erro {1}).", path, (int)result);
        return nullptr;
    }
    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Animator: falha ao carregar buffers de '{0}' (cgltf erro {1}).", path, (int)result);
        cgltf_free(data);
        return nullptr;
    }
    return BuildSkinFromGLTF(data, path);
}

Ref<SkinData> SkinData::CreateFromGLTFMemory(const void* data, std::size_t size) {
    cgltf_options options = {};
    cgltf_data* gltf = nullptr;
    cgltf_result result = cgltf_parse(&options, data, size, &gltf);
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Animator: falha ao parsear glTF em memória (cgltf erro {0}).", (int)result);
        return nullptr;
    }
    result = cgltf_load_buffers(&options, gltf, nullptr);
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Animator: falha ao carregar buffers do glTF em memória (cgltf erro {0}).", (int)result);
        cgltf_free(gltf);
        return nullptr;
    }
    return BuildSkinFromGLTF(gltf, "memória");
}

static Ref<SkinData> BuildSkinFromGLTF(cgltf_data* data, const std::string& label) {
    auto skin = CreateRef<SkinData>();
    if (data->skins_count == 0) {
        cgltf_free(data);
        KZ_CORE_WARN("Animator: '{0}' não tem skin — sem esqueleto pra animar.", label);
        return skin;
    }

    const cgltf_skin& gskin = data->skins[0];
    cgltf_size jointCount = gskin.joints_count;
    if (jointCount == 0 || jointCount > kMaxSkinJoints) {
        cgltf_free(data);
        KZ_CORE_ERROR("Animator: '{0}' tem {1} juntas (máx. {2}).", label, jointCount, kMaxSkinJoints);
        return nullptr;
    }

    skin->Joints.resize(jointCount);
    skin->Order.reserve(jointCount);

    for (cgltf_size i = 0; i < jointCount; ++i) {
        const cgltf_node* node = gskin.joints[i];
        SkinJoint& j = skin->Joints[i];
        j.Name = node->name ? node->name : "junta";
        j.T = { node->translation[0], node->translation[1], node->translation[2] };
        j.R = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
        j.S = { node->scale[0], node->scale[1], node->scale[2] };

        const cgltf_accessor* ibm = gskin.inverse_bind_matrices;
        if (ibm && i < ibm->count) {
            float m[16];
            cgltf_accessor_read_float(ibm, i, m, 16);
            j.InverseBind = glm::make_mat4(m);
        }

        if (node->parent) {
            for (cgltf_size p = 0; p < jointCount; ++p) {
                if (gskin.joints[p] == node->parent) { j.Parent = (int)p; break; }
            }
        }
    }

    std::vector<bool> used(jointCount, false);
    for (int pass = 0; pass < (int)jointCount; ++pass) {
        for (cgltf_size i = 0; i < jointCount; ++i) {
            if (used[i]) continue;
            int parent = skin->Joints[i].Parent;
            if (parent < 0 || used[(size_t)parent]) {
                used[i] = true;
                skin->Order.push_back((int)i);
            }
        }
    }

    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        const cgltf_animation& ganim = data->animations[a];
        AnimationClip clip;
        clip.Name = ganim.name ? ganim.name : "Anim" + std::to_string(a + 1);

        for (cgltf_size c = 0; c < ganim.channels_count; ++c) {
            const cgltf_animation_channel& chan = ganim.channels[c];
            const cgltf_animation_sampler& samp = *chan.sampler;
            if (chan.target_path == cgltf_animation_path_type_weights) continue;

            int joint = -1;
            for (cgltf_size j = 0; j < jointCount; ++j)
                if (gskin.joints[j] == chan.target_node) { joint = (int)j; break; }
            if (joint < 0) continue;

            AnimChannel ch;
            ch.Joint = joint;
            switch (chan.target_path) {
                case cgltf_animation_path_type_translation: ch.Type = AnimChannel::Path::Translation; break;
                case cgltf_animation_path_type_rotation:    ch.Type = AnimChannel::Path::Rotation; break;
                default:                                    ch.Type = AnimChannel::Path::Scale; break;
            }

            cgltf_size n = samp.input->count;
            int comps = (chan.target_path == cgltf_animation_path_type_rotation) ? 4 : 3;
            ch.Times.reserve(n);
            ch.Values.reserve(n);
            for (cgltf_size k = 0; k < n; ++k) {
                float t = 0.0f;
                cgltf_accessor_read_float(samp.input, k, &t, 1);
                ch.Times.push_back(t);
                glm::vec4 v(0.0f, 0.0f, 0.0f, 1.0f);
                cgltf_accessor_read_float(samp.output, k, &v.x, comps);
                ch.Values.push_back(v);
                if (k == n - 1 && t > clip.Duration) clip.Duration = t;
            }
            if (!ch.Times.empty()) clip.Channels.push_back(std::move(ch));
        }
        skin->Clips.push_back(std::move(clip));
    }

    cgltf_free(data);
    KZ_CORE_INFO("Skin carregada: {0} ({1} juntas, {2} animações).", label, jointCount, skin->Clips.size());
    return skin;
}

int SkinData::GetClipIndex(const std::string& name) const {
    for (size_t i = 0; i < Clips.size(); ++i)
        if (Clips[i].Name == name) return (int)i;
    return -1;
}

float SkinData::GetClipDuration(const std::string& name) const {
    int idx = GetClipIndex(name);
    return (idx >= 0) ? Clips[(size_t)idx].Duration : 0.0f;
}

bool SkinData::Evaluate(const std::string& clipName, float time, glm::mat4* outMatrices, int maxJoints) const {
    if (outMatrices == nullptr || Joints.empty()) return false;

    int n = (int)Joints.size();
    int count = std::min(n, maxJoints);

    std::vector<glm::vec3> t(n), s(n, glm::vec3(1.0f));
    std::vector<glm::quat> r(n);
    for (int i = 0; i < n; ++i) {
        t[i] = Joints[i].T;
        r[i] = Joints[i].R;
        s[i] = Joints[i].S;
    }

    int clipIdx = GetClipIndex(clipName);
    if (clipIdx >= 0) {
        const AnimationClip& clip = Clips[(size_t)clipIdx];
        for (const auto& ch : clip.Channels) {
            if (ch.Joint < 0 || ch.Joint >= n || ch.Times.empty()) continue;
            glm::vec4 v = SampleChannel(ch, time);
            switch (ch.Type) {
                case AnimChannel::Path::Translation: t[ch.Joint] = glm::vec3(v); break;
                case AnimChannel::Path::Rotation:    r[ch.Joint] = glm::quat(v.w, v.x, v.y, v.z); break;
                default:                             s[ch.Joint] = glm::vec3(v); break;
            }
        }
    }

    std::vector<glm::mat4> global(n);
    for (int idx = 0; idx < (int)Order.size(); ++idx) {
        int i = Order[(size_t)idx];
        glm::mat4 local = glm::translate(glm::mat4(1.0f), t[i]) * glm::mat4_cast(r[i]) * glm::scale(glm::mat4(1.0f), s[i]);
        global[i] = (Joints[i].Parent >= 0) ? global[Joints[i].Parent] * local : local;
    }

    for (int i = 0; i < count; ++i)
        outMatrices[i] = global[i] * Joints[i].InverseBind;
    return true;
}

bool SkinData::EvaluateGlobal(const std::string& clipName, float time, glm::mat4* outMatrices, int maxJoints) const {
    if (outMatrices == nullptr || Joints.empty()) return false;

    int n = (int)Joints.size();
    int count = std::min(n, maxJoints);

    std::vector<glm::vec3> t(n), s(n, glm::vec3(1.0f));
    std::vector<glm::quat> r(n);
    for (int i = 0; i < n; ++i) {
        t[i] = Joints[i].T;
        r[i] = Joints[i].R;
        s[i] = Joints[i].S;
    }

    int clipIdx = GetClipIndex(clipName);
    if (clipIdx >= 0) {
        const AnimationClip& clip = Clips[(size_t)clipIdx];
        for (const auto& ch : clip.Channels) {
            if (ch.Joint < 0 || ch.Joint >= n || ch.Times.empty()) continue;
            glm::vec4 v = SampleChannel(ch, time);
            switch (ch.Type) {
                case AnimChannel::Path::Translation: t[ch.Joint] = glm::vec3(v); break;
                case AnimChannel::Path::Rotation:    r[ch.Joint] = glm::quat(v.w, v.x, v.y, v.z); break;
                default:                             s[ch.Joint] = glm::vec3(v); break;
            }
        }
    }

    std::vector<glm::mat4> global(n);
    for (int idx = 0; idx < (int)Order.size(); ++idx) {
        int i = Order[(size_t)idx];
        glm::mat4 local = glm::translate(glm::mat4(1.0f), t[i]) * glm::mat4_cast(r[i]) * glm::scale(glm::mat4(1.0f), s[i]);
        global[i] = (Joints[i].Parent >= 0) ? global[Joints[i].Parent] * local : local;
    }

    for (int i = 0; i < count; ++i) outMatrices[i] = global[i];
    return true;
}

}
