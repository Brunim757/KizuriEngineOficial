#pragma once
// Serialização de componentes compartilhada entre SceneSerializer (.kzscene)
// e Prefab (.kzprefab). Fica em src/ (não em include/) de propósito: é
// detalhe de implementação, nenhum código de jogo deveria depender de
// nlohmann::json diretamente através da API pública da engine.
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/ecs/Scene.hpp"

#include <nlohmann/json.hpp>

namespace kizuri::detail {

inline nlohmann::json Vec3ToJson(const glm::vec3& v) { return { v.x, v.y, v.z }; }
inline nlohmann::json Vec4ToJson(const glm::vec4& v) { return { v.x, v.y, v.z, v.w }; }
inline glm::vec3 JsonToVec3(const nlohmann::json& j) { return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() }; }
inline glm::vec4 JsonToVec4(const nlohmann::json& j) { return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() }; }

// Serializa uma única entidade (sem os filhos — quem chama decide se
// percorre a hierarquia) para um objeto JSON, incluindo ID e Parent.
inline nlohmann::json SerializeEntityJson(Entity entity) {
    nlohmann::json je;
    je["ID"] = (uint64_t)entity.GetUUID();

    if (entity.HasComponent<RelationshipComponent>()) {
        auto& rel = entity.GetComponent<RelationshipComponent>();
        je["Parent"] = (uint64_t)rel.Parent;
    }

    je["Tag"] = entity.GetName();

    if (entity.HasComponent<TransformComponent>()) {
        auto& tc = entity.GetComponent<TransformComponent>();
        je["Transform"] = {
            { "Translation", Vec3ToJson(tc.Translation) },
            { "Rotation", Vec3ToJson(tc.Rotation) },
            { "Scale", Vec3ToJson(tc.Scale) },
        };
    }

    if (entity.HasComponent<SpriteRendererComponent>()) {
        auto& sc = entity.GetComponent<SpriteRendererComponent>();
        je["SpriteRenderer"] = {
            { "Color", Vec4ToJson(sc.Color) },
            { "TilingFactor", sc.TilingFactor }
        };
    }

    if (entity.HasComponent<CircleRendererComponent>()) {
        auto& cr = entity.GetComponent<CircleRendererComponent>();
        je["CircleRenderer"] = {
            { "Color", Vec4ToJson(cr.Color) },
            { "Thickness", cr.Thickness },
            { "Fade", cr.Fade }
        };
    }

    if (entity.HasComponent<CameraComponent>()) {
        auto& cc = entity.GetComponent<CameraComponent>();
        je["Camera"] = {
            { "Type", (int)cc.Type },
            { "OrthoSize", cc.OrthoSize },
            { "PerspectiveFOV", cc.PerspectiveFOV },
            { "NearClip", cc.NearClip },
            { "FarClip", cc.FarClip },
            { "Primary", cc.Primary }
        };
    }

    if (entity.HasComponent<Rigidbody2DComponent>()) {
        auto& rb = entity.GetComponent<Rigidbody2DComponent>();
        je["Rigidbody2D"] = { { "Type", (int)rb.Type }, { "FixedRotation", rb.FixedRotation } };
    }

    if (entity.HasComponent<BoxCollider2DComponent>()) {
        auto& bc = entity.GetComponent<BoxCollider2DComponent>();
        je["BoxCollider2D"] = {
            { "Offset", Vec3ToJson({ bc.Offset.x, bc.Offset.y, 0.0f }) },
            { "Size", Vec3ToJson({ bc.Size.x, bc.Size.y, 0.0f }) },
            { "Density", bc.Density }, { "Friction", bc.Friction }, { "Restitution", bc.Restitution }
        };
    }

    if (entity.HasComponent<Rigidbody3DComponent>()) {
        auto& rb = entity.GetComponent<Rigidbody3DComponent>();
        je["Rigidbody3D"] = { { "Type", (int)rb.Type }, { "Mass", rb.Mass } };
    }

    if (entity.HasComponent<BoxCollider3DComponent>()) {
        auto& bc = entity.GetComponent<BoxCollider3DComponent>();
        je["BoxCollider3D"] = { { "HalfExtents", Vec3ToJson(bc.HalfExtents) } };
    }

    if (entity.HasComponent<SphereCollider3DComponent>()) {
        auto& sc = entity.GetComponent<SphereCollider3DComponent>();
        je["SphereCollider3D"] = { { "Radius", sc.Radius } };
    }

    if (entity.HasComponent<LightComponent>()) {
        auto& lc = entity.GetComponent<LightComponent>();
        je["Light"] = {
            { "Type", (int)lc.Type }, { "Color", Vec3ToJson(lc.Color) }, { "Intensity", lc.Intensity },
            { "Range", lc.Range }, { "InnerConeDeg", lc.InnerConeDeg }, { "OuterConeDeg", lc.OuterConeDeg }
        };
    }

    if (entity.HasComponent<ParticleSystemComponent>()) {
        auto& pc = entity.GetComponent<ParticleSystemComponent>();
        je["ParticleSystem"] = {
            { "Playing", pc.Playing }, { "Additive", pc.Additive },
            { "EmissionRate", pc.EmissionRate }, { "MaxParticles", pc.MaxParticles },
            { "LifetimeMin", pc.LifetimeMin }, { "LifetimeMax", pc.LifetimeMax },
            { "VelocityMin", Vec3ToJson(pc.VelocityMin) }, { "VelocityMax", Vec3ToJson(pc.VelocityMax) },
            { "Gravity", Vec3ToJson(pc.Gravity) },
            { "StartColor", Vec4ToJson(pc.StartColor) }, { "EndColor", Vec4ToJson(pc.EndColor) },
            { "StartSize", pc.StartSize }, { "EndSize", pc.EndSize }
        };
    }

    if (entity.HasComponent<AudioSourceComponent>()) {
        auto& ac = entity.GetComponent<AudioSourceComponent>();
        je["AudioSource"] = {
            { "ClipPath", ac.ClipPath }, { "Loop", ac.Loop }, { "PlayOnStart", ac.PlayOnStart },
            { "Spatial", ac.Spatial }, { "Volume", ac.Volume },
            { "MinDistance", ac.MinDistance }, { "MaxDistance", ac.MaxDistance }
        };
    }

    if (entity.HasComponent<NativeScriptComponent>()) {
        auto& nsc = entity.GetComponent<NativeScriptComponent>();
        // Só vale a pena salvar quando veio de BindByName (Bind<T>() em
        // tempo de compilação não tem nome nenhum pra persistir — nesse
        // caso o script é religado pelo próprio código do jogo, não pela
        // cena). Uma classe vazia salva não faria sentido pra restaurar.
        if (!nsc.ClassName.empty())
            je["NativeScript"] = { { "ClassName", nsc.ClassName } };
    }

    return je;
}

// Cria uma entidade a partir do JSON gerado por SerializeEntityJson.
// 'uuid' força o UUID (usado pelo SceneSerializer, que quer preservar
// referências salvas); passe 0 para gerar um novo (usado pelo Prefab, que
// nunca deve reusar o UUID do original ao instanciar).
inline Entity DeserializeEntityJson(const nlohmann::json& je, Scene& scene, uint64_t uuid) {
    std::string tag = je.value("Tag", "Entidade");
    Entity entity = scene.CreateEntityWithUUID(uuid, tag);

    if (je.contains("Transform")) {
        auto& jt = je["Transform"];
        auto& tc = entity.GetComponent<TransformComponent>();
        tc.Translation = JsonToVec3(jt["Translation"]);
        tc.Rotation = JsonToVec3(jt["Rotation"]);
        tc.Scale = JsonToVec3(jt["Scale"]);
    }

    if (je.contains("SpriteRenderer")) {
        auto& js = je["SpriteRenderer"];
        auto& sc = entity.AddComponent<SpriteRendererComponent>();
        sc.Color = JsonToVec4(js["Color"]);
        sc.TilingFactor = js.value("TilingFactor", 1.0f);
    }

    if (je.contains("CircleRenderer")) {
        auto& jc = je["CircleRenderer"];
        auto& cr = entity.AddComponent<CircleRendererComponent>();
        cr.Color = JsonToVec4(jc["Color"]);
        cr.Thickness = jc.value("Thickness", 1.0f);
        cr.Fade = jc.value("Fade", 0.005f);
    }

    if (je.contains("Camera")) {
        auto& jc = je["Camera"];
        auto& cc = entity.AddComponent<CameraComponent>();
        cc.Type = (CameraComponent::ProjectionType)jc.value("Type", 0);
        cc.OrthoSize = jc.value("OrthoSize", 10.0f);
        cc.PerspectiveFOV = jc.value("PerspectiveFOV", 45.0f);
        cc.NearClip = jc.value("NearClip", 0.01f);
        cc.FarClip = jc.value("FarClip", 1000.0f);
        cc.Primary = jc.value("Primary", true);
    }

    if (je.contains("Rigidbody2D")) {
        auto& jr = je["Rigidbody2D"];
        auto& rb = entity.AddComponent<Rigidbody2DComponent>();
        rb.Type = (Rigidbody2DComponent::BodyType)jr.value("Type", 1);
        rb.FixedRotation = jr.value("FixedRotation", false);
    }

    if (je.contains("BoxCollider2D")) {
        auto& jb = je["BoxCollider2D"];
        auto& bc = entity.AddComponent<BoxCollider2DComponent>();
        auto off = JsonToVec3(jb["Offset"]); bc.Offset = { off.x, off.y };
        auto size = JsonToVec3(jb["Size"]); bc.Size = { size.x, size.y };
        bc.Density = jb.value("Density", 1.0f);
        bc.Friction = jb.value("Friction", 0.5f);
        bc.Restitution = jb.value("Restitution", 0.0f);
    }

    if (je.contains("Rigidbody3D")) {
        auto& jr = je["Rigidbody3D"];
        auto& rb = entity.AddComponent<Rigidbody3DComponent>();
        rb.Type = (Rigidbody3DComponent::BodyType)jr.value("Type", 1);
        rb.Mass = jr.value("Mass", 1.0f);
    }

    if (je.contains("BoxCollider3D")) {
        auto& jb = je["BoxCollider3D"];
        auto& bc = entity.AddComponent<BoxCollider3DComponent>();
        bc.HalfExtents = JsonToVec3(jb["HalfExtents"]);
    }

    if (je.contains("SphereCollider3D")) {
        auto& js = je["SphereCollider3D"];
        auto& sc = entity.AddComponent<SphereCollider3DComponent>();
        sc.Radius = js.value("Radius", 0.5f);
    }

    if (je.contains("Light")) {
        auto& jl = je["Light"];
        auto& lc = entity.AddComponent<LightComponent>();
        lc.Type = (LightType)jl.value("Type", 0);
        lc.Color = JsonToVec3(jl["Color"]);
        lc.Intensity = jl.value("Intensity", 1.0f);
        lc.Range = jl.value("Range", 10.0f);
        lc.InnerConeDeg = jl.value("InnerConeDeg", 20.0f);
        lc.OuterConeDeg = jl.value("OuterConeDeg", 30.0f);
    }

    if (je.contains("ParticleSystem")) {
        auto& jp = je["ParticleSystem"];
        auto& pc = entity.AddComponent<ParticleSystemComponent>();
        pc.Playing = jp.value("Playing", true);
        pc.Additive = jp.value("Additive", true);
        pc.EmissionRate = jp.value("EmissionRate", 30.0f);
        pc.MaxParticles = jp.value("MaxParticles", 500u);
        pc.LifetimeMin = jp.value("LifetimeMin", 0.6f);
        pc.LifetimeMax = jp.value("LifetimeMax", 1.2f);
        pc.VelocityMin = JsonToVec3(jp["VelocityMin"]);
        pc.VelocityMax = JsonToVec3(jp["VelocityMax"]);
        pc.Gravity = JsonToVec3(jp["Gravity"]);
        pc.StartColor = JsonToVec4(jp["StartColor"]);
        pc.EndColor = JsonToVec4(jp["EndColor"]);
        pc.StartSize = jp.value("StartSize", 0.15f);
        pc.EndSize = jp.value("EndSize", 0.4f);
    }

    if (je.contains("AudioSource")) {
        auto& ja = je["AudioSource"];
        auto& ac = entity.AddComponent<AudioSourceComponent>();
        ac.ClipPath = ja.value("ClipPath", "");
        ac.Loop = ja.value("Loop", false);
        ac.PlayOnStart = ja.value("PlayOnStart", true);
        ac.Spatial = ja.value("Spatial", true);
        ac.Volume = ja.value("Volume", 1.0f);
        ac.MinDistance = ja.value("MinDistance", 1.0f);
        ac.MaxDistance = ja.value("MaxDistance", 50.0f);
    }

    if (je.contains("NativeScript")) {
        auto& jn = je["NativeScript"];
        auto& nsc = entity.AddComponent<NativeScriptComponent>();
        nsc.BindByName(jn.value("ClassName", ""));
    }

    return entity;
}

// Aplica um snapshot (gerado por SerializeEntityJson) numa entidade que já
// EXISTE — diferente de DeserializeEntityJson, que sempre cria uma nova.
// É o coração do undo/redo de edição de propriedade: adiciona/atualiza
// cada componente presente no JSON e remove qualquer componente opcional
// que a entidade tenha hoje mas que não apareça no snapshot (cobre o caso
// de desfazer um "Adicionar Componente" ou refazer um "Remover
// Componente"). ID e Parent são ignorados de propósito — essa função nunca
// mexe em identidade nem em hierarquia, só nos componentes de dados.
inline void ApplyEntityStateJson(Entity entity, const nlohmann::json& je) {
    entity.GetComponent<TagComponent>().Tag = je.value("Tag", entity.GetName());

    if (je.contains("Transform")) {
        auto& jt = je["Transform"];
        auto& tc = entity.GetComponent<TransformComponent>();
        tc.Translation = JsonToVec3(jt["Translation"]);
        tc.Rotation = JsonToVec3(jt["Rotation"]);
        tc.Scale = JsonToVec3(jt["Scale"]);
    }

    if (je.contains("SpriteRenderer")) {
        auto& js = je["SpriteRenderer"];
        auto& sc = entity.HasComponent<SpriteRendererComponent>()
            ? entity.GetComponent<SpriteRendererComponent>()
            : entity.AddComponent<SpriteRendererComponent>();
        sc.Color = JsonToVec4(js["Color"]);
        sc.TilingFactor = js.value("TilingFactor", 1.0f);
    } else if (entity.HasComponent<SpriteRendererComponent>()) {
        entity.RemoveComponent<SpriteRendererComponent>();
    }

    if (je.contains("CircleRenderer")) {
        auto& jc = je["CircleRenderer"];
        auto& cr = entity.HasComponent<CircleRendererComponent>()
            ? entity.GetComponent<CircleRendererComponent>()
            : entity.AddComponent<CircleRendererComponent>();
        cr.Color = JsonToVec4(jc["Color"]);
        cr.Thickness = jc.value("Thickness", 1.0f);
        cr.Fade = jc.value("Fade", 0.005f);
    } else if (entity.HasComponent<CircleRendererComponent>()) {
        entity.RemoveComponent<CircleRendererComponent>();
    }

    if (je.contains("Camera")) {
        auto& jc = je["Camera"];
        auto& cc = entity.HasComponent<CameraComponent>()
            ? entity.GetComponent<CameraComponent>()
            : entity.AddComponent<CameraComponent>();
        cc.Type = (CameraComponent::ProjectionType)jc.value("Type", 0);
        cc.OrthoSize = jc.value("OrthoSize", 10.0f);
        cc.PerspectiveFOV = jc.value("PerspectiveFOV", 45.0f);
        cc.NearClip = jc.value("NearClip", 0.01f);
        cc.FarClip = jc.value("FarClip", 1000.0f);
        cc.Primary = jc.value("Primary", true);
    } else if (entity.HasComponent<CameraComponent>()) {
        entity.RemoveComponent<CameraComponent>();
    }

    if (je.contains("Rigidbody2D")) {
        auto& jr = je["Rigidbody2D"];
        auto& rb = entity.HasComponent<Rigidbody2DComponent>()
            ? entity.GetComponent<Rigidbody2DComponent>()
            : entity.AddComponent<Rigidbody2DComponent>();
        rb.Type = (Rigidbody2DComponent::BodyType)jr.value("Type", 1);
        rb.FixedRotation = jr.value("FixedRotation", false);
    } else if (entity.HasComponent<Rigidbody2DComponent>()) {
        entity.RemoveComponent<Rigidbody2DComponent>();
    }

    if (je.contains("BoxCollider2D")) {
        auto& jb = je["BoxCollider2D"];
        auto& bc = entity.HasComponent<BoxCollider2DComponent>()
            ? entity.GetComponent<BoxCollider2DComponent>()
            : entity.AddComponent<BoxCollider2DComponent>();
        auto off = JsonToVec3(jb["Offset"]); bc.Offset = { off.x, off.y };
        auto size = JsonToVec3(jb["Size"]); bc.Size = { size.x, size.y };
        bc.Density = jb.value("Density", 1.0f);
        bc.Friction = jb.value("Friction", 0.5f);
        bc.Restitution = jb.value("Restitution", 0.0f);
    } else if (entity.HasComponent<BoxCollider2DComponent>()) {
        entity.RemoveComponent<BoxCollider2DComponent>();
    }

    if (je.contains("Rigidbody3D")) {
        auto& jr = je["Rigidbody3D"];
        auto& rb = entity.HasComponent<Rigidbody3DComponent>()
            ? entity.GetComponent<Rigidbody3DComponent>()
            : entity.AddComponent<Rigidbody3DComponent>();
        rb.Type = (Rigidbody3DComponent::BodyType)jr.value("Type", 1);
        rb.Mass = jr.value("Mass", 1.0f);
    } else if (entity.HasComponent<Rigidbody3DComponent>()) {
        entity.RemoveComponent<Rigidbody3DComponent>();
    }

    if (je.contains("BoxCollider3D")) {
        auto& jb = je["BoxCollider3D"];
        auto& bc = entity.HasComponent<BoxCollider3DComponent>()
            ? entity.GetComponent<BoxCollider3DComponent>()
            : entity.AddComponent<BoxCollider3DComponent>();
        bc.HalfExtents = JsonToVec3(jb["HalfExtents"]);
    } else if (entity.HasComponent<BoxCollider3DComponent>()) {
        entity.RemoveComponent<BoxCollider3DComponent>();
    }

    if (je.contains("SphereCollider3D")) {
        auto& js = je["SphereCollider3D"];
        auto& sc = entity.HasComponent<SphereCollider3DComponent>()
            ? entity.GetComponent<SphereCollider3DComponent>()
            : entity.AddComponent<SphereCollider3DComponent>();
        sc.Radius = js.value("Radius", 0.5f);
    } else if (entity.HasComponent<SphereCollider3DComponent>()) {
        entity.RemoveComponent<SphereCollider3DComponent>();
    }

    if (je.contains("Light")) {
        auto& jl = je["Light"];
        auto& lc = entity.HasComponent<LightComponent>()
            ? entity.GetComponent<LightComponent>()
            : entity.AddComponent<LightComponent>();
        lc.Type = (LightType)jl.value("Type", 0);
        lc.Color = JsonToVec3(jl["Color"]);
        lc.Intensity = jl.value("Intensity", 1.0f);
        lc.Range = jl.value("Range", 10.0f);
        lc.InnerConeDeg = jl.value("InnerConeDeg", 20.0f);
        lc.OuterConeDeg = jl.value("OuterConeDeg", 30.0f);
    } else if (entity.HasComponent<LightComponent>()) {
        entity.RemoveComponent<LightComponent>();
    }

    if (je.contains("ParticleSystem")) {
        auto& jp = je["ParticleSystem"];
        auto& pc = entity.HasComponent<ParticleSystemComponent>()
            ? entity.GetComponent<ParticleSystemComponent>()
            : entity.AddComponent<ParticleSystemComponent>();
        pc.Playing = jp.value("Playing", true);
        pc.Additive = jp.value("Additive", true);
        pc.EmissionRate = jp.value("EmissionRate", 30.0f);
        pc.MaxParticles = jp.value("MaxParticles", 500u);
        pc.LifetimeMin = jp.value("LifetimeMin", 0.6f);
        pc.LifetimeMax = jp.value("LifetimeMax", 1.2f);
        pc.VelocityMin = JsonToVec3(jp["VelocityMin"]);
        pc.VelocityMax = JsonToVec3(jp["VelocityMax"]);
        pc.Gravity = JsonToVec3(jp["Gravity"]);
        pc.StartColor = JsonToVec4(jp["StartColor"]);
        pc.EndColor = JsonToVec4(jp["EndColor"]);
        pc.StartSize = jp.value("StartSize", 0.15f);
        pc.EndSize = jp.value("EndSize", 0.4f);
    } else if (entity.HasComponent<ParticleSystemComponent>()) {
        entity.RemoveComponent<ParticleSystemComponent>();
    }

    if (je.contains("AudioSource")) {
        auto& ja = je["AudioSource"];
        auto& ac = entity.HasComponent<AudioSourceComponent>()
            ? entity.GetComponent<AudioSourceComponent>()
            : entity.AddComponent<AudioSourceComponent>();
        ac.ClipPath = ja.value("ClipPath", "");
        ac.Loop = ja.value("Loop", false);
        ac.PlayOnStart = ja.value("PlayOnStart", true);
        ac.Spatial = ja.value("Spatial", true);
        ac.Volume = ja.value("Volume", 1.0f);
        ac.MinDistance = ja.value("MinDistance", 1.0f);
        ac.MaxDistance = ja.value("MaxDistance", 50.0f);
    } else if (entity.HasComponent<AudioSourceComponent>()) {
        entity.RemoveComponent<AudioSourceComponent>();
    }

    if (je.contains("NativeScript")) {
        std::string className = je["NativeScript"].value("ClassName", "");
        if (!entity.HasComponent<NativeScriptComponent>())
            entity.AddComponent<NativeScriptComponent>();
        entity.GetComponent<NativeScriptComponent>().BindByName(className);
    } else if (entity.HasComponent<NativeScriptComponent>()) {
        entity.RemoveComponent<NativeScriptComponent>();
    }
}

} // namespace kizuri::detail
