#pragma once

#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/project/Project.hpp"

#include <nlohmann/json.hpp>

namespace kizuri::detail {

inline std::string ResolveSerializedPath(const std::string& path) {
    return Project::ResolvePath(path);
}

inline void RestoreGLTFTextureMaps(MeshRendererComponent& mr) {
    const std::string& src = mr.MeshSource;
    if (src.find(".glb") == std::string::npos && src.find(".gltf") == std::string::npos) return;
    Material restored = Mesh::ExtractMaterialFromGLTF(ResolveSerializedPath(src));
    auto& mat = mr.MeshMaterial;
    if (!mat.AlbedoMap && restored.AlbedoMap) mat.AlbedoMap = restored.AlbedoMap;
    if (!mat.NormalMap && restored.NormalMap) mat.NormalMap = restored.NormalMap;
    if (!mat.MetallicRoughnessMap && restored.MetallicRoughnessMap) mat.MetallicRoughnessMap = restored.MetallicRoughnessMap;
    if (!mat.EmissiveMap && restored.EmissiveMap) mat.EmissiveMap = restored.EmissiveMap;
    if (!mat.HeightMap && restored.HeightMap) mat.HeightMap = restored.HeightMap;
}

inline nlohmann::json Vec2ToJson(const glm::vec2& v) { return { v.x, v.y }; }
inline nlohmann::json Vec3ToJson(const glm::vec3& v) { return { v.x, v.y, v.z }; }
inline nlohmann::json Vec4ToJson(const glm::vec4& v) { return { v.x, v.y, v.z, v.w }; }
inline static glm::vec2 JsonToVec2(const nlohmann::json& j) {
    return j.is_array() ? glm::vec2(j[0].get<float>(), j[1].get<float>()) : glm::vec2(0.0f);
}
inline glm::vec3 JsonToVec3(const nlohmann::json& j) { return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() }; }
inline glm::vec4 JsonToVec4(const nlohmann::json& j) { return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() }; }

inline void ApplyTimelineJson(nlohmann::json::const_reference jtl, TimelineComponent& tl) {
    tl.Playing = jtl.value("Playing", true);
    tl.Loop = jtl.value("Loop", true);
    tl.Time = jtl.value("Time", 0.0f);
    tl.Speed = jtl.value("Speed", 1.0f);
    tl.Keyframes.clear();
    if (jtl.contains("Keyframes") && jtl["Keyframes"].is_array()) {
        for (auto& jk : jtl["Keyframes"]) {
            TimelineComponent::Keyframe k;
            k.Time = jk.value("Time", 0.0f);
            k.Position = jk.contains("Position") ? JsonToVec3(jk["Position"]) : glm::vec3(0.0f);
            k.Rotation = jk.contains("Rotation") ? JsonToVec3(jk["Rotation"]) : glm::vec3(0.0f);
            k.Scale = jk.contains("Scale") ? JsonToVec3(jk["Scale"]) : glm::vec3(1.0f);
            tl.Keyframes.push_back(std::move(k));
        }
    }
}
inline void ApplyLODJson(nlohmann::json::const_reference jlod, LODComponent& lod) {
    lod.DistanceMultiplier = jlod.value("DistanceMultiplier", 1.0f);
    lod.Levels.clear();
    if (jlod.contains("Levels") && jlod["Levels"].is_array()) {
        for (auto& jl : jlod["Levels"]) {
            LODComponent::Level l;
            l.MeshSource = jl.value("MeshSource", "");
            l.Distance = jl.value("Distance", 50.0f);
            if (!l.MeshSource.empty()) l.MeshAsset = Mesh::FromSource(ResolveSerializedPath(l.MeshSource));
            lod.Levels.push_back(std::move(l));
        }
    }
}

 inline nlohmann::json SerializeEntityJson(Entity entity) {
    nlohmann::json je;
    je["ID"] = (uint64_t)entity.GetUUID();
    je["Active"] = entity.GetComponent<IDComponent>().Active;

    if (entity.HasComponent<RelationshipComponent>()) {
        auto& rel = entity.GetComponent<RelationshipComponent>();
        je["Parent"] = (uint64_t)rel.Parent;
    }

    je["Tag"] = entity.GetName();
    if (entity.HasComponent<TagComponent>()) {
        auto& tag = entity.GetComponent<TagComponent>();
        je["TagLayer"] = tag.Layer;
        je["TagCollisionMask"] = tag.CollisionMask;
    }

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
            { "TilingFactor", sc.TilingFactor },
            { "TexturePath", sc.TexturePath },
            { "SortingLayer", sc.SortingLayer },
            { "FlipX", sc.FlipX }, { "FlipY", sc.FlipY }
        };
    }

    if (entity.HasComponent<CircleRendererComponent>()) {
        auto& cr = entity.GetComponent<CircleRendererComponent>();
        je["CircleRenderer"] = {
            { "Color", Vec4ToJson(cr.Color) },
            { "Thickness", cr.Thickness },
            { "Fade", cr.Fade },
            { "SortingLayer", cr.SortingLayer }
        };
    }

    if (entity.HasComponent<TextComponent>()) {
        auto& tc = entity.GetComponent<TextComponent>();
        je["Text"] = {
            { "Text", tc.Text },
            { "Color", Vec4ToJson(tc.Color) },
            { "FontSize", tc.FontSize },
            { "Alignment", (int)tc.Alignment },
            { "SortingLayer", tc.SortingLayer }
        };
    }

    if (entity.HasComponent<FoliageComponent>()) {
        auto& fc = entity.GetComponent<FoliageComponent>();
        je["Foliage"] = { { "MeshSource", fc.MeshSource }, { "AreaSize", Vec2ToJson(fc.AreaSize) },
                          { "HeightScale", fc.HeightScale }, { "Count", fc.Count },
                          { "ScaleMin", fc.ScaleMin }, { "ScaleMax", fc.ScaleMax },
                          { "Seed", fc.Seed }, { "AvoidCenter", fc.AvoidCenter },
                          { "Color", Vec4ToJson(fc.Color) } };
    }

    if (entity.HasComponent<OccluderComponent>()) {
        auto& oc = entity.GetComponent<OccluderComponent>();
        je["Occluder"] = { { "HalfExtents", Vec3ToJson(oc.HalfExtents) },
                           { "MaxOcclusionDistance", oc.MaxOcclusionDistance } };
    }

    if (entity.HasComponent<DecalComponent>()) {
        auto& dc = entity.GetComponent<DecalComponent>();
        je["Decal"] = { { "TexturePath", dc.TexturePath }, { "Color", Vec4ToJson(dc.Color) },
                        { "SortingLayer", dc.SortingLayer } };
    }

    if (entity.HasComponent<SpriteAnimationComponent>()) {
        auto& ac = entity.GetComponent<SpriteAnimationComponent>();
        je["SpriteAnimation"] = {
            { "SheetPath", ac.SheetPath },
            { "FramesPerRow", ac.FramesPerRow },
            { "TotalFrames", ac.TotalFrames },
            { "FPS", ac.FPS },
            { "Loop", ac.Loop },
            { "SortingLayer", ac.SortingLayer }
        };
    }

    if (entity.HasComponent<TilemapComponent>()) {
        auto& tmc = entity.GetComponent<TilemapComponent>();
        je["Tilemap"] = {
            { "AtlasPath", tmc.AtlasPath },
            { "AtlasColumns", tmc.AtlasColumns },
            { "AtlasRows", tmc.AtlasRows },
            { "MapWidth", tmc.MapWidth },
            { "MapHeight", tmc.MapHeight },
            { "TileSize", Vec3ToJson({ tmc.TileSize.x, tmc.TileSize.y, 0.0f }) },
            { "Tiles", tmc.Tiles },
            { "SolidTileValues", tmc.SolidTileValues },
            { "SortingLayer", tmc.SortingLayer }
        };
    }

    if (entity.HasComponent<MeshRendererComponent>()) {
        auto& mr = entity.GetComponent<MeshRendererComponent>();
        auto& mat = mr.MeshMaterial;
        je["MeshRenderer"] = {
            { "MeshSource", mr.MeshSource },
            { "LightmapPath", mr.LightmapPath },
            { "Albedo", Vec3ToJson(mat.Albedo) },
            { "Metallic", mat.Metallic },
            { "Roughness", mat.Roughness },
            { "AO", mat.AO },
            { "Emissive", Vec3ToJson(mat.Emissive) },
            { "EmissiveStrength", mat.EmissiveStrength },
            { "AlbedoMapPath", mat.AlbedoMapPath },
            { "NormalMapPath", mat.NormalMapPath },
            { "MetallicRoughnessMapPath", mat.MetallicRoughnessMapPath },
            { "EmissiveMapPath", mat.EmissiveMapPath },
            { "HeightMapPath", mat.HeightMapPath },
            { "HeightScale", mat.HeightScale },
            { "PlanarReflect", mat.PlanarReflect }
        };
    }

    if (entity.HasComponent<LODComponent>()) {
        auto& lod = entity.GetComponent<LODComponent>();
        nlohmann::json levels = nlohmann::json::array();
        for (auto& l : lod.Levels)
            levels.push_back({ { "MeshSource", l.MeshSource }, { "Distance", l.Distance } });
        je["LOD"] = { { "DistanceMultiplier", lod.DistanceMultiplier }, { "Levels", levels } };
    }

    if (entity.HasComponent<TerrainComponent>()) {
        auto& t = entity.GetComponent<TerrainComponent>();
        nlohmann::json jlod = nlohmann::json::array();
        for (auto& lod : t.LODLevels)
            jlod.push_back({ { "Segments", lod.Segments }, { "Distance", lod.Distance } });
        je["Terrain"] = { { "Segments", t.Segments }, { "Size", t.Size },
                          { "HeightScale", t.HeightScale }, { "Seed", t.Seed },
                          { "LODLevels", jlod }, { "LODDistanceMultiplier", t.LODDistanceMultiplier } };
    }

    if (entity.HasComponent<AnimationBlendComponent>()) {
        auto& ab = entity.GetComponent<AnimationBlendComponent>();
        je["AnimationBlend"] = { { "ClipA", ab.ClipA }, { "ClipB", ab.ClipB },
                                 { "BlendWeight", ab.BlendWeight }, { "UseBlend", ab.UseBlend } };
    }

    if (entity.HasComponent<TwoBoneIKComponent>()) {
        auto& ik = entity.GetComponent<TwoBoneIKComponent>();
        je["TwoBoneIK"] = { { "RootBone", ik.RootBone }, { "MidBone", ik.MidBone },
                            { "TipBone", ik.TipBone }, { "Target", Vec3ToJson(ik.Target) },
                            { "Weight", ik.Weight } };
    }

    if (entity.HasComponent<CharacterControllerComponent>()) {
        auto& cc = entity.GetComponent<CharacterControllerComponent>();
        je["CharacterController"] = { { "Speed", cc.Speed }, { "Gravity", cc.Gravity },
                                      { "Radius", cc.Radius }, { "Height", cc.Height },
                                      { "StepOffset", cc.StepOffset } };
    }

    if (entity.HasComponent<TimelineComponent>()) {
        auto& tl = entity.GetComponent<TimelineComponent>();
        nlohmann::json kf = nlohmann::json::array();
        for (auto& k : tl.Keyframes)
            kf.push_back({ { "Time", k.Time }, { "Position", Vec3ToJson(k.Position) },
                           { "Rotation", Vec3ToJson(k.Rotation) }, { "Scale", Vec3ToJson(k.Scale) } });
        je["Timeline"] = { { "Playing", tl.Playing }, { "Loop", tl.Loop },
                           { "Time", tl.Time }, { "Speed", tl.Speed }, { "Keyframes", kf } };
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

    if (entity.HasComponent<CameraFollowComponent>()) {
        auto& cf = entity.GetComponent<CameraFollowComponent>();
        je["CameraFollow"] = {
            { "TargetName", cf.TargetName },
            { "Offset", Vec3ToJson(cf.Offset) },
            { "Smoothness", cf.Smoothness },
            { "FollowRotation", cf.FollowRotation },
            { "UseWorldOffset", cf.UseWorldOffset }
        };
    }

    if (entity.HasComponent<UICanvasComponent>()) {
        auto& uc = entity.GetComponent<UICanvasComponent>();
        je["UICanvas"] = { { "OrthoSize", uc.OrthoSize } };
    }

    if (entity.HasComponent<UIRectComponent>()) {
        auto& ur = entity.GetComponent<UIRectComponent>();
        je["UIRect"] = {
            { "Position", Vec3ToJson({ ur.Position.x, ur.Position.y, 0.0f }) },
            { "Size", Vec3ToJson({ ur.Size.x, ur.Size.y, 0.0f }) },
            { "Color", Vec4ToJson(ur.Color) }
        };
    }

    if (entity.HasComponent<UIButtonComponent>()) {
        je["UIButton"] = {};
    }

    if (entity.HasComponent<Rigidbody2DComponent>()) {
        auto& rb = entity.GetComponent<Rigidbody2DComponent>();
        je["Rigidbody2D"] = { { "Type", (int)rb.Type }, { "FixedRotation", rb.FixedRotation }, { "GravityScale", rb.GravityScale } };
    }

    if (entity.HasComponent<BoxCollider2DComponent>()) {
        auto& bc = entity.GetComponent<BoxCollider2DComponent>();
        je["BoxCollider2D"] = {
            { "Offset", Vec3ToJson({ bc.Offset.x, bc.Offset.y, 0.0f }) },
            { "Size", Vec3ToJson({ bc.Size.x, bc.Size.y, 0.0f }) },
            { "Density", bc.Density }, { "Friction", bc.Friction }, { "Restitution", bc.Restitution }
        };
    }

    if (entity.HasComponent<CircleCollider2DComponent>()) {
        auto& cc = entity.GetComponent<CircleCollider2DComponent>();
        je["CircleCollider2D"] = {
            { "Offset", Vec3ToJson({ cc.Offset.x, cc.Offset.y, 0.0f }) },
            { "Radius", cc.Radius },
            { "Density", cc.Density }, { "Friction", cc.Friction }, { "Restitution", cc.Restitution }
        };
    }

    if (entity.HasComponent<Rigidbody3DComponent>()) {
        auto& rb = entity.GetComponent<Rigidbody3DComponent>();
        je["Rigidbody3D"] = { { "Type", (int)rb.Type }, { "Mass", rb.Mass },
                              { "GravityScale", rb.GravityScale },
                              { "LinearDamping", rb.LinearDamping },
                              { "AngularDamping", rb.AngularDamping } };
    }

    if (entity.HasComponent<BoxCollider3DComponent>()) {
        auto& bc = entity.GetComponent<BoxCollider3DComponent>();
        je["BoxCollider3D"] = { { "HalfExtents", Vec3ToJson(bc.HalfExtents) } };
    }

    if (entity.HasComponent<SphereCollider3DComponent>()) {
        auto& sc = entity.GetComponent<SphereCollider3DComponent>();
        je["SphereCollider3D"] = { { "Radius", sc.Radius } };
    }

    if (entity.HasComponent<MeshColliderComponent>()) {
        auto& mc = entity.GetComponent<MeshColliderComponent>();
        je["MeshCollider"] = { { "MeshPath", mc.MeshPath }, { "MaxPoints", mc.MaxPoints } };
    }

    if (entity.HasComponent<LightComponent>()) {
        auto& lc = entity.GetComponent<LightComponent>();
        je["Light"] = {
            { "Type", (int)lc.Type }, { "Color", Vec3ToJson(lc.Color) }, { "Intensity", lc.Intensity },
            { "Range", lc.Range }, { "InnerConeDeg", lc.InnerConeDeg }, { "OuterConeDeg", lc.OuterConeDeg },
            { "CastsShadow", lc.CastsShadow }
        };
    }

    if (entity.HasComponent<NavGridComponent>()) {
        auto& ng = entity.GetComponent<NavGridComponent>();
        je["NavGrid"] = { { "Origin", Vec3ToJson(ng.Origin) }, { "Width", ng.Width },
                          { "Depth", ng.Depth }, { "CellSize", ng.CellSize },
                          { "AutoBuild", ng.AutoBuild } };
    }
    if (entity.HasComponent<NavObstacleComponent>()) {
        auto& no = entity.GetComponent<NavObstacleComponent>();
        je["NavObstacle"] = { { "HalfExtents", Vec3ToJson(no.HalfExtents) } };
    }
    if (entity.HasComponent<NavAgentComponent>()) {
        auto& na = entity.GetComponent<NavAgentComponent>();
        je["NavAgent"] = { { "Speed", na.Speed }, { "TurnSpeed", na.TurnSpeed },
                           { "StopDistance", na.StopDistance }, { "Radius", na.Radius },
                           { "FaceMovement", na.FaceMovement }, { "Enabled", na.Enabled } };
    }
    if (entity.HasComponent<EnemyAIComponent>()) {
        auto& ai = entity.GetComponent<EnemyAIComponent>();
        nlohmann::json patrol = nlohmann::json::array();
        for (auto& p : ai.PatrolPoints) patrol.push_back(Vec3ToJson(p));
        je["EnemyAI"] = {
            { "InitialState", (int)ai.InitialState },
            { "SightRange", ai.SightRange }, { "LoseRange", ai.LoseRange },
            { "ChaseRange", ai.ChaseRange }, { "AttackCooldown", ai.AttackCooldown },
            { "AttackDamage", ai.AttackDamage }, { "PatrolWait", ai.PatrolWait },
            { "TargetTag", ai.TargetTag }, { "PatrolPoints", patrol }
        };
    }

    if (entity.HasComponent<ChunkWorldComponent>()) {
        auto& cw = entity.GetComponent<ChunkWorldComponent>();
        je["ChunkWorld"] = {
            { "ChunkSize", cw.ChunkSize },
            { "LoadRadius", cw.LoadRadius },
            { "UnloadGrace", cw.UnloadGrace },
            { "ChunksPerFrame", cw.ChunksPerFrame },
            { "AutoSaveInterval", cw.AutoSaveInterval },
            { "ChunkFolder", cw.ChunkFolder },
            { "TargetTag", cw.TargetTag }
        };
    }

    if (entity.HasComponent<ChunkEntityComponent>()) {
        auto& ce = entity.GetComponent<ChunkEntityComponent>();
        je["ChunkEntity"] = { { "ChunkX", ce.ChunkX }, { "ChunkZ", ce.ChunkZ }, { "ChunkSeed", ce.ChunkSeed } };
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
            { "StartSize", pc.StartSize }, { "EndSize", pc.EndSize },
            { "TexturePath", pc.TexturePath }
        };
    }

    if (entity.HasComponent<AudioSourceComponent>()) {
        auto& ac = entity.GetComponent<AudioSourceComponent>();
        je["AudioSource"] = {
            { "ClipPath", ac.ClipPath }, { "Loop", ac.Loop }, { "PlayOnStart", ac.PlayOnStart },
            { "Spatial", ac.Spatial }, { "Reverb", ac.Reverb }, { "Volume", ac.Volume }, { "Group", ac.Group },
            { "MinDistance", ac.MinDistance }, { "MaxDistance", ac.MaxDistance }
        };
    }

    if (entity.HasComponent<NativeScriptComponent>()) {
        auto& nsc = entity.GetComponent<NativeScriptComponent>();

        if (!nsc.ClassName.empty())
            je["NativeScript"] = { { "ClassName", nsc.ClassName } };
    }

    if (entity.HasComponent<AnimatorComponent>()) {
        auto& ac = entity.GetComponent<AnimatorComponent>();
        je["Animator"] = {
            { "MeshPath", ac.MeshPath }, { "ClipName", ac.ClipName },
            { "Playing", ac.Playing }, { "Loop", ac.Loop },
            { "Speed", ac.Speed }, { "Time", ac.Time }
        };
    }

    if (entity.HasComponent<AnimatorStateMachineComponent>()) {
        const auto& sm = entity.GetComponent<AnimatorStateMachineComponent>();
        nlohmann::json jstates = nlohmann::json::array();
        for (const auto& st : sm.States)
            jstates.push_back({ { "Name", st.Name }, { "Clip", st.Clip }, { "Speed", st.Speed }, { "Loop", st.Loop } });
        nlohmann::json jtrans = nlohmann::json::array();
        for (const auto& tr : sm.Transitions)
            jtrans.push_back({ { "From", tr.From }, { "To", tr.To }, { "BlendTime", tr.BlendTime } });
        je["AnimatorSM"] = { { "States", std::move(jstates) }, { "Transitions", std::move(jtrans) } };
    }

    return je;
}

inline Entity DeserializeEntityJson(const nlohmann::json& je, Scene& scene, uint64_t uuid) {
    std::string tag = je.value("Tag", "Entidade");
    Entity entity = scene.CreateEntityWithUUID(uuid, tag);
    if (je.contains("Active")) entity.GetComponent<IDComponent>().Active = je.value("Active", true);
    if (je.contains("TagLayer")) entity.GetComponent<TagComponent>().Layer = je.value("TagLayer", 0);
    if (je.contains("TagCollisionMask")) entity.GetComponent<TagComponent>().CollisionMask = je.value("TagCollisionMask", 0xFFFFFFFFu);

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
        sc.TexturePath = js.value("TexturePath", "");
        sc.SortingLayer = js.value("SortingLayer", 0);
        sc.FlipX = js.value("FlipX", false);
        sc.FlipY = js.value("FlipY", false);
        if (!sc.TexturePath.empty()) sc.Texture = Texture2D::Create(ResolveSerializedPath(sc.TexturePath));
    }

    if (je.contains("LOD")) {
        auto& lod = entity.HasComponent<LODComponent>()
            ? entity.GetComponent<LODComponent>()
            : entity.AddComponent<LODComponent>();
        ApplyLODJson(je["LOD"], lod);
    } else if (entity.HasComponent<LODComponent>() && !entity.HasComponent<MeshRendererComponent>()) {
        entity.RemoveComponent<LODComponent>();
    }

    if (je.contains("Terrain")) {
        auto& jt = je["Terrain"];
        auto& t = entity.HasComponent<TerrainComponent>()
            ? entity.GetComponent<TerrainComponent>()
            : entity.AddComponent<TerrainComponent>();
        t.Segments = jt.value("Segments", 64u);
        t.Size = jt.value("Size", 100.0f);
        t.HeightScale = jt.value("HeightScale", 5.0f);
        t.Seed = jt.value("Seed", 1u);
        t.LODDistanceMultiplier = jt.value("LODDistanceMultiplier", 1.0f);
        t.LODLevels.clear();
        if (jt.contains("LODLevels"))
            for (auto& jlod : jt["LODLevels"])
                t.LODLevels.push_back({ jlod.value("Segments", 16u), jlod.value("Distance", 50.0f) });
        t.Regenerate();
        if (entity.HasComponent<MeshRendererComponent>())
            entity.GetComponent<MeshRendererComponent>().MeshAsset = t.GeneratedMesh;
    } else if (entity.HasComponent<TerrainComponent>()) {
        entity.RemoveComponent<TerrainComponent>();
    }

    if (je.contains("Timeline")) {
        auto& tl = entity.HasComponent<TimelineComponent>()
            ? entity.GetComponent<TimelineComponent>()
            : entity.AddComponent<TimelineComponent>();
        ApplyTimelineJson(je["Timeline"], tl);
    } else if (entity.HasComponent<TimelineComponent>()) {
        entity.RemoveComponent<TimelineComponent>();
    }

        if (je.contains("AnimationBlend")) {
        auto& jab = je["AnimationBlend"];
        auto& ab = entity.AddComponent<AnimationBlendComponent>();
        ab.ClipA = jab.value("ClipA", "");
        ab.ClipB = jab.value("ClipB", "");
        ab.BlendWeight = jab.value("BlendWeight", 0.0f);
        ab.UseBlend = jab.value("UseBlend", true);
    }

    if (je.contains("TwoBoneIK")) {
        auto& jik = je["TwoBoneIK"];
        auto& ik = entity.AddComponent<TwoBoneIKComponent>();
        ik.RootBone = jik.value("RootBone", "");
        ik.MidBone = jik.value("MidBone", "");
        ik.TipBone = jik.value("TipBone", "");
        ik.Target = JsonToVec3(jik["Target"]);
        ik.Weight = jik.value("Weight", 1.0f);
    }

    if (je.contains("AnimationBlend")) {
        auto& jab = je["AnimationBlend"];
        auto& ab = entity.AddComponent<AnimationBlendComponent>();
        ab.ClipA = jab.value("ClipA", "");
        ab.ClipB = jab.value("ClipB", "");
        ab.BlendWeight = jab.value("BlendWeight", 0.0f);
        ab.UseBlend = jab.value("UseBlend", true);
    }

    if (je.contains("TwoBoneIK")) {
        auto& jik = je["TwoBoneIK"];
        auto& ik = entity.AddComponent<TwoBoneIKComponent>();
        ik.RootBone = jik.value("RootBone", "");
        ik.MidBone = jik.value("MidBone", "");
        ik.TipBone = jik.value("TipBone", "");
        ik.Target = JsonToVec3(jik["Target"]);
        ik.Weight = jik.value("Weight", 1.0f);
    }

if (je.contains("CharacterController")) {
        auto& jc = je["CharacterController"];
        auto& cc = entity.HasComponent<CharacterControllerComponent>()
            ? entity.GetComponent<CharacterControllerComponent>()
            : entity.AddComponent<CharacterControllerComponent>();
        cc.Speed = jc.value("Speed", 6.0f);
        cc.Gravity = jc.value("Gravity", -20.0f);
        cc.Radius = jc.value("Radius", 0.4f);
        cc.Height = jc.value("Height", 1.8f);
        cc.StepOffset = jc.value("StepOffset", 0.3f);
    } else if (entity.HasComponent<CharacterControllerComponent>()) {
        entity.RemoveComponent<CharacterControllerComponent>();
    }

    if (je.contains("CircleRenderer")) {
        auto& jc = je["CircleRenderer"];
        auto& cr = entity.AddComponent<CircleRendererComponent>();
        cr.Color = JsonToVec4(jc["Color"]);
        cr.Thickness = jc.value("Thickness", 1.0f);
        cr.Fade = jc.value("Fade", 0.005f);
        cr.SortingLayer = jc.value("SortingLayer", 0);
    }

    if (je.contains("Text")) {
        auto& jt = je["Text"];
        auto& tc = entity.AddComponent<TextComponent>();
        tc.Text = jt.value("Text", "Texto");
        tc.Color = JsonToVec4(jt["Color"]);
        tc.FontSize = jt.value("FontSize", 48.0f);
        tc.Alignment = (TextAlignment)jt.value("Alignment", 0);
        tc.SortingLayer = jt.value("SortingLayer", 0);
    }

                if (je.contains("Foliage")) {
        auto& jf = je["Foliage"];
        auto& fc = entity.HasComponent<FoliageComponent>()
            ? entity.GetComponent<FoliageComponent>()
            : entity.AddComponent<FoliageComponent>();
        fc.MeshSource = jf.value("MeshSource", "builtin:cone");
        fc.AreaSize = JsonToVec2(jf["AreaSize"]);
        fc.HeightScale = jf.value("HeightScale", 1.0f);
        fc.Count = jf.value("Count", 200u);
        fc.ScaleMin = jf.value("ScaleMin", 0.6f);
        fc.ScaleMax = jf.value("ScaleMax", 1.3f);
        fc.Seed = jf.value("Seed", 42u);
        fc.AvoidCenter = jf.value("AvoidCenter", true);
        fc.Color = JsonToVec4(jf["Color"]);
        fc.Regenerate();
    } else if (entity.HasComponent<FoliageComponent>()) {
        entity.RemoveComponent<FoliageComponent>();
    }

if (je.contains("Occluder")) {
        auto& jo = je["Occluder"];
        auto& oc = entity.HasComponent<OccluderComponent>()
            ? entity.GetComponent<OccluderComponent>()
            : entity.AddComponent<OccluderComponent>();
        oc.HalfExtents = JsonToVec3(jo["HalfExtents"]);
        oc.MaxOcclusionDistance = jo.value("MaxOcclusionDistance", 60.0f);
    } else if (entity.HasComponent<OccluderComponent>()) {
        entity.RemoveComponent<OccluderComponent>();
    }

if (je.contains("Decal")) {
        auto& jd = je["Decal"];
        auto& dc = entity.HasComponent<DecalComponent>()
            ? entity.GetComponent<DecalComponent>()
            : entity.AddComponent<DecalComponent>();
        dc.TexturePath = jd.value("TexturePath", "");
        dc.Color = JsonToVec4(jd["Color"]);
        dc.SortingLayer = jd.value("SortingLayer", 0);
        if (!dc.TexturePath.empty()) dc.Texture = Texture2D::Create(ResolveSerializedPath(dc.TexturePath));
    } else if (entity.HasComponent<DecalComponent>()) {
        entity.RemoveComponent<DecalComponent>();
    }

if (je.contains("SpriteAnimation")) {
        auto& ja = je["SpriteAnimation"];
        auto& ac = entity.AddComponent<SpriteAnimationComponent>();
        ac.SheetPath = ja.value("SheetPath", "");
        ac.FramesPerRow = ja.value("FramesPerRow", 1u);
        ac.TotalFrames = ja.value("TotalFrames", 1u);
        ac.FPS = ja.value("FPS", 12.0f);
        ac.Loop = ja.value("Loop", true);
        ac.SortingLayer = ja.value("SortingLayer", 0);
        if (!ac.SheetPath.empty()) ac.SheetTexture = Texture2D::Create(ResolveSerializedPath(ac.SheetPath));
    }

    if (je.contains("Tilemap")) {
        auto& jtm = je["Tilemap"];
        auto& tmc = entity.AddComponent<TilemapComponent>();
        tmc.AtlasPath = jtm.value("AtlasPath", "");
        tmc.AtlasColumns = jtm.value("AtlasColumns", 1u);
        tmc.AtlasRows = jtm.value("AtlasRows", 1u);
        tmc.MapWidth = jtm.value("MapWidth", 0u);
        tmc.MapHeight = jtm.value("MapHeight", 0u);
        auto ts = JsonToVec3(jtm.value("TileSize", nlohmann::json::array({ 1.0f, 1.0f, 0.0f })));
        tmc.TileSize = { ts.x, ts.y };
        tmc.Tiles = jtm.value("Tiles", std::vector<uint32_t>{});
        tmc.SolidTileValues = jtm.value("SolidTileValues", std::vector<uint32_t>{});
        tmc.SortingLayer = jtm.value("SortingLayer", 0);
        if (!tmc.AtlasPath.empty()) tmc.AtlasTexture = Texture2D::Create(ResolveSerializedPath(tmc.AtlasPath));
    }

    if (je.contains("MeshRenderer")) {
        auto& jm = je["MeshRenderer"];
        auto& mr = entity.AddComponent<MeshRendererComponent>();
        mr.MeshSource = jm.value("MeshSource", "builtin:cube");
        mr.LightmapPath = jm.value("LightmapPath", "");
        if (!mr.LightmapPath.empty()) mr.LightmapTexture = Texture2D::Create(kizuri::Project::ResolvePath(mr.LightmapPath));
        mr.MeshAsset = Mesh::FromSource(ResolveSerializedPath(mr.MeshSource));
        auto& mat = mr.MeshMaterial;
        mat.Albedo = JsonToVec3(jm["Albedo"]);
        mat.Metallic = jm.value("Metallic", 0.0f);
        mat.Roughness = jm.value("Roughness", 0.5f);
        mat.AO = jm.value("AO", 1.0f);
        mat.Emissive = jm.contains("Emissive") ? JsonToVec3(jm["Emissive"]) : glm::vec3(0.0f);
        mat.EmissiveStrength = jm.value("EmissiveStrength", 0.0f);
        mat.AlbedoMapPath = jm.value("AlbedoMapPath", "");
        mat.NormalMapPath = jm.value("NormalMapPath", "");
        mat.MetallicRoughnessMapPath = jm.value("MetallicRoughnessMapPath", "");
        mat.EmissiveMapPath = jm.value("EmissiveMapPath", "");
        mat.HeightMapPath = jm.value("HeightMapPath", "");
        mat.HeightScale = jm.value("HeightScale", 0.08f);
        mat.PlanarReflect = jm.value("PlanarReflect", false);
        if (!mat.AlbedoMapPath.empty()) mat.AlbedoMap = Texture2D::Create(ResolveSerializedPath(mat.AlbedoMapPath));
        if (!mat.NormalMapPath.empty()) mat.NormalMap = Texture2D::Create(ResolveSerializedPath(mat.NormalMapPath));
        if (!mat.MetallicRoughnessMapPath.empty()) mat.MetallicRoughnessMap = Texture2D::Create(ResolveSerializedPath(mat.MetallicRoughnessMapPath));
        if (!mat.EmissiveMapPath.empty()) mat.EmissiveMap = Texture2D::Create(ResolveSerializedPath(mat.EmissiveMapPath));
        if (!mat.HeightMapPath.empty()) mat.HeightMap = Texture2D::Create(ResolveSerializedPath(mat.HeightMapPath));
        RestoreGLTFTextureMaps(mr);
    }

    if (je.contains("LOD")) {
        auto& lod = entity.AddComponent<LODComponent>();
        ApplyLODJson(je["LOD"], lod);
    }

    if (je.contains("Terrain")) {
        auto& jt = je["Terrain"];
        auto& t = entity.AddComponent<TerrainComponent>();
        t.Segments = jt.value("Segments", 64u);
        t.Size = jt.value("Size", 100.0f);
        t.HeightScale = jt.value("HeightScale", 5.0f);
        t.Seed = jt.value("Seed", 1u);
        t.LODDistanceMultiplier = jt.value("LODDistanceMultiplier", 1.0f);
        t.LODLevels.clear();
        if (jt.contains("LODLevels"))
            for (auto& jlod : jt["LODLevels"])
                t.LODLevels.push_back({ jlod.value("Segments", 16u), jlod.value("Distance", 50.0f) });
        t.Regenerate();
        if (entity.HasComponent<MeshRendererComponent>())
            entity.GetComponent<MeshRendererComponent>().MeshAsset = t.GeneratedMesh;
    }

    if (je.contains("Timeline")) {
        auto& tl = entity.AddComponent<TimelineComponent>();
        ApplyTimelineJson(je["Timeline"], tl);
    }

    if (je.contains("CharacterController")) {
        auto& jc = je["CharacterController"];
        auto& cc = entity.AddComponent<CharacterControllerComponent>();
        cc.Speed = jc.value("Speed", 6.0f);
        cc.Gravity = jc.value("Gravity", -20.0f);
        cc.Radius = jc.value("Radius", 0.4f);
        cc.Height = jc.value("Height", 1.8f);
        cc.StepOffset = jc.value("StepOffset", 0.3f);
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

    if (je.contains("CameraFollow")) {
        auto& jf = je["CameraFollow"];
        auto& cf = entity.AddComponent<CameraFollowComponent>();
        cf.TargetName = jf.value("TargetName", "");
        cf.Offset = JsonToVec3(jf["Offset"]);
        cf.Smoothness = jf.value("Smoothness", 8.0f);
        cf.FollowRotation = jf.value("FollowRotation", true);
        cf.UseWorldOffset = jf.value("UseWorldOffset", false);
    }

    if (je.contains("UICanvas")) {
        auto& ju = je["UICanvas"];
        auto& uc = entity.AddComponent<UICanvasComponent>();
        uc.OrthoSize = ju.value("OrthoSize", 10.0f);
    }

    if (je.contains("UIRect")) {
        auto& jr = je["UIRect"];
        auto& ur = entity.AddComponent<UIRectComponent>();
        auto pos = JsonToVec3(jr["Position"]); ur.Position = { pos.x, pos.y };
        auto size = JsonToVec3(jr["Size"]); ur.Size = { size.x, size.y };
        ur.Color = JsonToVec4(jr["Color"]);
    }

    if (je.contains("UIButton")) {
        entity.AddComponent<UIButtonComponent>();
    }

    if (je.contains("Rigidbody2D")) {
        auto& jr = je["Rigidbody2D"];
        auto& rb = entity.AddComponent<Rigidbody2DComponent>();
        rb.Type = (Rigidbody2DComponent::BodyType)jr.value("Type", 1);
        rb.FixedRotation = jr.value("FixedRotation", false);
        rb.GravityScale = jr.value("GravityScale", 1.0f);
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

    if (je.contains("CircleCollider2D")) {
        auto& jc = je["CircleCollider2D"];
        auto& cc = entity.AddComponent<CircleCollider2DComponent>();
        auto off = JsonToVec3(jc["Offset"]); cc.Offset = { off.x, off.y };
        cc.Radius = jc.value("Radius", 0.5f);
        cc.Density = jc.value("Density", 1.0f);
        cc.Friction = jc.value("Friction", 0.5f);
        cc.Restitution = jc.value("Restitution", 0.0f);
    }

    if (je.contains("Rigidbody3D")) {
        auto& jr = je["Rigidbody3D"];
        auto& rb = entity.AddComponent<Rigidbody3DComponent>();
        rb.Type = (Rigidbody3DComponent::BodyType)jr.value("Type", 1);
        rb.Mass = jr.value("Mass", 1.0f);
        rb.GravityScale = jr.value("GravityScale", 1.0f);
        rb.LinearDamping = jr.value("LinearDamping", 0.0f);
        rb.AngularDamping = jr.value("AngularDamping", 0.0f);
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

    if (je.contains("MeshCollider")) {
        auto& jm = je["MeshCollider"];
        auto& mc = entity.AddComponent<MeshColliderComponent>();
        mc.MeshPath = jm.value("MeshPath", "");
        mc.MaxPoints = jm.value("MaxPoints", 96u);
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
        pc.TexturePath = jp.value("TexturePath", "");
        if (!pc.TexturePath.empty()) pc.Texture = Texture2D::Create(ResolveSerializedPath(pc.TexturePath));
    }

    if (je.contains("NavGrid")) {
        auto& jn = je["NavGrid"];
        auto& ng = entity.AddComponent<NavGridComponent>();
        ng.Origin = JsonToVec3(jn["Origin"]);
        ng.Width = jn.value("Width", 40u);
        ng.Depth = jn.value("Depth", 40u);
        ng.CellSize = jn.value("CellSize", 1.0f);
        ng.AutoBuild = jn.value("AutoBuild", true);
    }
    if (je.contains("NavObstacle")) {
        auto& jo = je["NavObstacle"];
        auto& no = entity.AddComponent<NavObstacleComponent>();
        no.HalfExtents = JsonToVec3(jo["HalfExtents"]);
    }
    if (je.contains("NavAgent")) {
        auto& ja = je["NavAgent"];
        auto& na = entity.AddComponent<NavAgentComponent>();
        na.Speed = ja.value("Speed", 4.0f);
        na.TurnSpeed = ja.value("TurnSpeed", 8.0f);
        na.StopDistance = ja.value("StopDistance", 0.3f);
        na.Radius = ja.value("Radius", 0.3f);
        na.FaceMovement = ja.value("FaceMovement", true);
        na.Enabled = ja.value("Enabled", true);
    }
    if (je.contains("EnemyAI")) {
        auto& ji = je["EnemyAI"];
        auto& ai = entity.AddComponent<EnemyAIComponent>();
        ai.InitialState = (EnemyAIComponent::State)ji.value("InitialState", 0);
        ai.m_State = ai.InitialState;
        ai.SightRange = ji.value("SightRange", 12.0f);
        ai.LoseRange = ji.value("LoseRange", 18.0f);
        ai.ChaseRange = ji.value("ChaseRange", 8.0f);
        ai.AttackCooldown = ji.value("AttackCooldown", 1.2f);
        ai.AttackDamage = ji.value("AttackDamage", 1.0f);
        ai.PatrolWait = ji.value("PatrolWait", 1.5f);
        ai.TargetTag = ji.value("TargetTag", "Jogador");
        ai.PatrolPoints.clear();
        if (ji.contains("PatrolPoints") && ji["PatrolPoints"].is_array()) {
            for (auto& jp : ji["PatrolPoints"]) ai.PatrolPoints.push_back(JsonToVec3(jp));
        }
    }

    if (je.contains("ChunkWorld")) {
        auto& jc = je["ChunkWorld"];
        auto& cw = entity.AddComponent<ChunkWorldComponent>();
        cw.ChunkSize = jc.value("ChunkSize", 64.0f);
        cw.LoadRadius = jc.value("LoadRadius", 2);
        cw.UnloadGrace = jc.value("UnloadGrace", 1);
        cw.ChunksPerFrame = jc.value("ChunksPerFrame", 3);
        cw.AutoSaveInterval = jc.value("AutoSaveInterval", 0.0f);
        cw.ChunkFolder = jc.value("ChunkFolder", std::string("Chunks"));
        cw.TargetTag = jc.value("TargetTag", std::string("Jogador"));
    }

    if (je.contains("ChunkEntity")) {
        auto& jc = je["ChunkEntity"];
        auto& ce = entity.AddComponent<ChunkEntityComponent>();
        ce.ChunkX = jc.value("ChunkX", 0);
        ce.ChunkZ = jc.value("ChunkZ", 0);
        ce.ChunkSeed = jc.value("ChunkSeed", (uint64_t)0);
    }

    if (je.contains("AudioSource")) {
        auto& ja = je["AudioSource"];
        auto& ac = entity.AddComponent<AudioSourceComponent>();
        ac.ClipPath = ja.value("ClipPath", "");
        ac.Loop = ja.value("Loop", false);
        ac.PlayOnStart = ja.value("PlayOnStart", true);
        ac.Reverb = ja.value("Reverb", false);
        ac.Spatial = ja.value("Spatial", true);
        ac.Volume = ja.value("Volume", 1.0f);
        ac.Group = ja.value("Group", 0);
        ac.MinDistance = ja.value("MinDistance", 1.0f);
        ac.MaxDistance = ja.value("MaxDistance", 50.0f);
    }

    if (je.contains("NativeScript")) {
        auto& jn = je["NativeScript"];
        auto& nsc = entity.AddComponent<NativeScriptComponent>();
        nsc.BindByName(jn.value("ClassName", ""));
    }

    if (je.contains("Animator")) {
        auto& ja = je["Animator"];
        auto& ac = entity.AddComponent<AnimatorComponent>();
        ac.MeshPath = ja.value("MeshPath", "");
        ac.ClipName = ja.value("ClipName", "");
        ac.Playing = ja.value("Playing", true);
        ac.Loop = ja.value("Loop", true);
        ac.Speed = ja.value("Speed", 1.0f);
        ac.Time = ja.value("Time", 0.0f);

    }

    if (je.contains("AnimatorSM")) {
        const auto& jsm = je["AnimatorSM"];
        auto& sm = entity.AddComponent<AnimatorStateMachineComponent>();
        if (jsm.contains("States") && jsm["States"].is_array()) {
            for (const auto& js : jsm["States"]) {
                AnimStateDef st;
                st.Name = js.value("Name", "");
                st.Clip = js.value("Clip", "");
                st.Speed = js.value("Speed", 1.0f);
                st.Loop = js.value("Loop", true);
                if (!st.Name.empty()) sm.States.push_back(std::move(st));
            }
        }
        if (jsm.contains("Transitions") && jsm["Transitions"].is_array()) {
            for (const auto& jt : jsm["Transitions"]) {
                AnimTransitionDef tr;
                tr.From = jt.value("From", -1);
                tr.To = jt.value("To", 0);
                tr.BlendTime = jt.value("BlendTime", 0.3f);
                sm.Transitions.push_back(tr);
            }
        }
        if (!sm.States.empty()) sm.CurrentState = 0;
    }

    return entity;
}

inline void ApplyEntityStateJson(Entity entity, const nlohmann::json& je) {
    auto& tagc = entity.GetComponent<TagComponent>();
    tagc.Tag = je.value("Tag", entity.GetName());
    tagc.Layer = je.value("TagLayer", tagc.Layer);
    tagc.CollisionMask = je.value("TagCollisionMask", tagc.CollisionMask);

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
        sc.TexturePath = js.value("TexturePath", "");
        sc.Texture = sc.TexturePath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(sc.TexturePath));
    } else if (entity.HasComponent<SpriteRendererComponent>()) {
        entity.RemoveComponent<SpriteRendererComponent>();
    }

    if (je.contains("Text")) {
        auto& jt = je["Text"];
        auto& tc = entity.HasComponent<TextComponent>()
            ? entity.GetComponent<TextComponent>()
            : entity.AddComponent<TextComponent>();
        tc.Text = jt.value("Text", "Texto");
        tc.Color = JsonToVec4(jt["Color"]);
        tc.FontSize = jt.value("FontSize", 48.0f);
        tc.Alignment = (TextAlignment)jt.value("Alignment", 0);
    } else if (entity.HasComponent<TextComponent>()) {
        entity.RemoveComponent<TextComponent>();
    }

                if (je.contains("Foliage")) {
        auto& jf = je["Foliage"];
        auto& fc = entity.HasComponent<FoliageComponent>()
            ? entity.GetComponent<FoliageComponent>()
            : entity.AddComponent<FoliageComponent>();
        fc.MeshSource = jf.value("MeshSource", "builtin:cone");
        fc.AreaSize = JsonToVec2(jf["AreaSize"]);
        fc.HeightScale = jf.value("HeightScale", 1.0f);
        fc.Count = jf.value("Count", 200u);
        fc.ScaleMin = jf.value("ScaleMin", 0.6f);
        fc.ScaleMax = jf.value("ScaleMax", 1.3f);
        fc.Seed = jf.value("Seed", 42u);
        fc.AvoidCenter = jf.value("AvoidCenter", true);
        fc.Color = JsonToVec4(jf["Color"]);
        fc.Regenerate();
    } else if (entity.HasComponent<FoliageComponent>()) {
        entity.RemoveComponent<FoliageComponent>();
    }

if (je.contains("Occluder")) {
        auto& jo = je["Occluder"];
        auto& oc = entity.HasComponent<OccluderComponent>()
            ? entity.GetComponent<OccluderComponent>()
            : entity.AddComponent<OccluderComponent>();
        oc.HalfExtents = JsonToVec3(jo["HalfExtents"]);
        oc.MaxOcclusionDistance = jo.value("MaxOcclusionDistance", 60.0f);
    } else if (entity.HasComponent<OccluderComponent>()) {
        entity.RemoveComponent<OccluderComponent>();
    }

if (je.contains("Decal")) {
        auto& jd = je["Decal"];
        auto& dc = entity.HasComponent<DecalComponent>()
            ? entity.GetComponent<DecalComponent>()
            : entity.AddComponent<DecalComponent>();
        dc.TexturePath = jd.value("TexturePath", "");
        dc.Color = JsonToVec4(jd["Color"]);
        dc.SortingLayer = jd.value("SortingLayer", 0);
        if (!dc.TexturePath.empty()) dc.Texture = Texture2D::Create(ResolveSerializedPath(dc.TexturePath));
    } else if (entity.HasComponent<DecalComponent>()) {
        entity.RemoveComponent<DecalComponent>();
    }

if (je.contains("SpriteAnimation")) {
        auto& ja = je["SpriteAnimation"];
        auto& ac = entity.HasComponent<SpriteAnimationComponent>()
            ? entity.GetComponent<SpriteAnimationComponent>()
            : entity.AddComponent<SpriteAnimationComponent>();
        ac.SheetPath = ja.value("SheetPath", "");
        ac.FramesPerRow = ja.value("FramesPerRow", 1u);
        ac.TotalFrames = ja.value("TotalFrames", 1u);
        ac.FPS = ja.value("FPS", 12.0f);
        ac.Loop = ja.value("Loop", true);
        ac.SheetTexture = ac.SheetPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(ac.SheetPath));
    } else if (entity.HasComponent<SpriteAnimationComponent>()) {
        entity.RemoveComponent<SpriteAnimationComponent>();
    }

    if (je.contains("Tilemap")) {
        auto& jtm = je["Tilemap"];
        auto& tmc = entity.HasComponent<TilemapComponent>()
            ? entity.GetComponent<TilemapComponent>()
            : entity.AddComponent<TilemapComponent>();
        tmc.AtlasPath = jtm.value("AtlasPath", "");
        tmc.AtlasColumns = jtm.value("AtlasColumns", 1u);
        tmc.AtlasRows = jtm.value("AtlasRows", 1u);
        tmc.MapWidth = jtm.value("MapWidth", 0u);
        tmc.MapHeight = jtm.value("MapHeight", 0u);
        auto ts = JsonToVec3(jtm.value("TileSize", nlohmann::json::array({ 1.0f, 1.0f, 0.0f })));
        tmc.TileSize = { ts.x, ts.y };
        tmc.Tiles = jtm.value("Tiles", std::vector<uint32_t>{});
        tmc.SolidTileValues = jtm.value("SolidTileValues", std::vector<uint32_t>{});
        tmc.AtlasTexture = tmc.AtlasPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(tmc.AtlasPath));
    } else if (entity.HasComponent<TilemapComponent>()) {
        entity.RemoveComponent<TilemapComponent>();
    }

    if (je.contains("MeshRenderer")) {
        auto& jm = je["MeshRenderer"];
        auto& mr = entity.HasComponent<MeshRendererComponent>()
            ? entity.GetComponent<MeshRendererComponent>()
            : entity.AddComponent<MeshRendererComponent>();
        mr.MeshSource = jm.value("MeshSource", "builtin:cube");
        mr.LightmapPath = jm.value("LightmapPath", "");
        if (!mr.LightmapPath.empty()) mr.LightmapTexture = Texture2D::Create(kizuri::Project::ResolvePath(mr.LightmapPath));
        mr.MeshAsset = Mesh::FromSource(ResolveSerializedPath(mr.MeshSource));
        auto& mat = mr.MeshMaterial;
        mat.Albedo = JsonToVec3(jm["Albedo"]);
        mat.Metallic = jm.value("Metallic", 0.0f);
        mat.Roughness = jm.value("Roughness", 0.5f);
        mat.AO = jm.value("AO", 1.0f);
        mat.Emissive = jm.contains("Emissive") ? JsonToVec3(jm["Emissive"]) : glm::vec3(0.0f);
        mat.EmissiveStrength = jm.value("EmissiveStrength", 0.0f);
        mat.AlbedoMapPath = jm.value("AlbedoMapPath", "");
        mat.NormalMapPath = jm.value("NormalMapPath", "");
        mat.MetallicRoughnessMapPath = jm.value("MetallicRoughnessMapPath", "");
        mat.EmissiveMapPath = jm.value("EmissiveMapPath", "");
        mat.HeightMapPath = jm.value("HeightMapPath", "");
        mat.HeightScale = jm.value("HeightScale", 0.08f);
        mat.PlanarReflect = jm.value("PlanarReflect", false);
        mat.AlbedoMap = mat.AlbedoMapPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(mat.AlbedoMapPath));
        mat.NormalMap = mat.NormalMapPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(mat.NormalMapPath));
        mat.MetallicRoughnessMap = mat.MetallicRoughnessMapPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(mat.MetallicRoughnessMapPath));
        mat.EmissiveMap = mat.EmissiveMapPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(mat.EmissiveMapPath));
        mat.HeightMap = mat.HeightMapPath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(mat.HeightMapPath));
        RestoreGLTFTextureMaps(mr);
    } else if (entity.HasComponent<MeshRendererComponent>()) {
        entity.RemoveComponent<MeshRendererComponent>();
    }

    if (je.contains("LOD")) {
        auto& lod = entity.HasComponent<LODComponent>()
            ? entity.GetComponent<LODComponent>()
            : entity.AddComponent<LODComponent>();
        ApplyLODJson(je["LOD"], lod);
    } else if (entity.HasComponent<LODComponent>() && !entity.HasComponent<MeshRendererComponent>()) {
        entity.RemoveComponent<LODComponent>();
    }

    if (je.contains("Terrain")) {
        auto& jt = je["Terrain"];
        auto& t = entity.HasComponent<TerrainComponent>()
            ? entity.GetComponent<TerrainComponent>()
            : entity.AddComponent<TerrainComponent>();
        t.Segments = jt.value("Segments", 64u);
        t.Size = jt.value("Size", 100.0f);
        t.HeightScale = jt.value("HeightScale", 5.0f);
        t.Seed = jt.value("Seed", 1u);
        t.LODDistanceMultiplier = jt.value("LODDistanceMultiplier", 1.0f);
        t.LODLevels.clear();
        if (jt.contains("LODLevels"))
            for (auto& jlod : jt["LODLevels"])
                t.LODLevels.push_back({ jlod.value("Segments", 16u), jlod.value("Distance", 50.0f) });
        t.Regenerate();
        if (entity.HasComponent<MeshRendererComponent>())
            entity.GetComponent<MeshRendererComponent>().MeshAsset = t.GeneratedMesh;
    } else if (entity.HasComponent<TerrainComponent>()) {
        entity.RemoveComponent<TerrainComponent>();
    }

    if (je.contains("Timeline")) {
        auto& tl = entity.HasComponent<TimelineComponent>()
            ? entity.GetComponent<TimelineComponent>()
            : entity.AddComponent<TimelineComponent>();
        ApplyTimelineJson(je["Timeline"], tl);
    } else if (entity.HasComponent<TimelineComponent>()) {
        entity.RemoveComponent<TimelineComponent>();
    }

    if (je.contains("CharacterController")) {
        auto& jc = je["CharacterController"];
        auto& cc = entity.HasComponent<CharacterControllerComponent>()
            ? entity.GetComponent<CharacterControllerComponent>()
            : entity.AddComponent<CharacterControllerComponent>();
        cc.Speed = jc.value("Speed", 6.0f);
        cc.Gravity = jc.value("Gravity", -20.0f);
        cc.Radius = jc.value("Radius", 0.4f);
        cc.Height = jc.value("Height", 1.8f);
        cc.StepOffset = jc.value("StepOffset", 0.3f);
    } else if (entity.HasComponent<CharacterControllerComponent>()) {
        entity.RemoveComponent<CharacterControllerComponent>();
    }

    if (je.contains("AnimationBlend")) {
        auto& jab = je["AnimationBlend"];
        auto& ab = entity.HasComponent<AnimationBlendComponent>()
            ? entity.GetComponent<AnimationBlendComponent>()
            : entity.AddComponent<AnimationBlendComponent>();
        ab.ClipA = jab.value("ClipA", "");
        ab.ClipB = jab.value("ClipB", "");
        ab.BlendWeight = jab.value("BlendWeight", 0.0f);
        ab.UseBlend = jab.value("UseBlend", true);
    } else if (entity.HasComponent<AnimationBlendComponent>()) {
        entity.RemoveComponent<AnimationBlendComponent>();
    }

    if (je.contains("TwoBoneIK")) {
        auto& jik = je["TwoBoneIK"];
        auto& ik = entity.HasComponent<TwoBoneIKComponent>()
            ? entity.GetComponent<TwoBoneIKComponent>()
            : entity.AddComponent<TwoBoneIKComponent>();
        ik.RootBone = jik.value("RootBone", "");
        ik.MidBone = jik.value("MidBone", "");
        ik.TipBone = jik.value("TipBone", "");
        ik.Target = JsonToVec3(jik["Target"]);
        ik.Weight = jik.value("Weight", 1.0f);
    } else if (entity.HasComponent<TwoBoneIKComponent>()) {
        entity.RemoveComponent<TwoBoneIKComponent>();
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

    if (je.contains("CameraFollow")) {
        auto& jf = je["CameraFollow"];
        auto& cf = entity.HasComponent<CameraFollowComponent>()
            ? entity.GetComponent<CameraFollowComponent>()
            : entity.AddComponent<CameraFollowComponent>();
        cf.TargetName = jf.value("TargetName", "");
        cf.Offset = JsonToVec3(jf["Offset"]);
        cf.Smoothness = jf.value("Smoothness", 8.0f);
        cf.FollowRotation = jf.value("FollowRotation", true);
        cf.UseWorldOffset = jf.value("UseWorldOffset", false);
        cf.m_HasStart = false;
    } else if (entity.HasComponent<CameraFollowComponent>()) {
        entity.RemoveComponent<CameraFollowComponent>();
    }

    if (je.contains("Rigidbody2D")) {
        auto& jr = je["Rigidbody2D"];
        auto& rb = entity.HasComponent<Rigidbody2DComponent>()
            ? entity.GetComponent<Rigidbody2DComponent>()
            : entity.AddComponent<Rigidbody2DComponent>();
        rb.Type = (Rigidbody2DComponent::BodyType)jr.value("Type", 1);
        rb.FixedRotation = jr.value("FixedRotation", false);
        rb.GravityScale = jr.value("GravityScale", 1.0f);
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
        rb.GravityScale = jr.value("GravityScale", 1.0f);
        rb.LinearDamping = jr.value("LinearDamping", 0.0f);
        rb.AngularDamping = jr.value("AngularDamping", 0.0f);
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
        lc.CastsShadow = jl.value("CastsShadow", false);
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
        pc.TexturePath = jp.value("TexturePath", "");
        pc.Texture = pc.TexturePath.empty() ? nullptr : Texture2D::Create(ResolveSerializedPath(pc.TexturePath));
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
        ac.Group = ja.value("Group", 0);
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

    if (je.contains("AnimatorSM")) {
        auto& sm = entity.HasComponent<AnimatorStateMachineComponent>()
            ? entity.GetComponent<AnimatorStateMachineComponent>()
            : entity.AddComponent<AnimatorStateMachineComponent>();
        sm.States.clear();
        sm.Transitions.clear();
        sm.CurrentState = -1;
        sm.m_TransitionFrom = -1;
        sm.m_TransitionTime = 0.0f;
        if (je["AnimatorSM"].contains("States") && je["AnimatorSM"]["States"].is_array()) {
            for (const auto& js : je["AnimatorSM"]["States"]) {
                AnimStateDef st;
                st.Name = js.value("Name", "");
                st.Clip = js.value("Clip", "");
                st.Speed = js.value("Speed", 1.0f);
                st.Loop = js.value("Loop", true);
                if (!st.Name.empty()) sm.States.push_back(std::move(st));
            }
        }
        if (je["AnimatorSM"].contains("Transitions") && je["AnimatorSM"]["Transitions"].is_array()) {
            for (const auto& jt : je["AnimatorSM"]["Transitions"]) {
                AnimTransitionDef tr;
                tr.From = jt.value("From", -1);
                tr.To = jt.value("To", 0);
                tr.BlendTime = jt.value("BlendTime", 0.3f);
                sm.Transitions.push_back(tr);
            }
        }
        if (!sm.States.empty()) sm.CurrentState = 0;
    } else if (entity.HasComponent<AnimatorStateMachineComponent>()) {
        entity.RemoveComponent<AnimatorStateMachineComponent>();
    }

    if (je.contains("Animator")) {
        auto& ja = je["Animator"];
        auto& ac = entity.HasComponent<AnimatorComponent>()
            ? entity.GetComponent<AnimatorComponent>()
            : entity.AddComponent<AnimatorComponent>();
        ac.MeshPath = ja.value("MeshPath", "");
        ac.ClipName = ja.value("ClipName", "");
        ac.Playing = ja.value("Playing", true);
        ac.Loop = ja.value("Loop", true);
        ac.Speed = ja.value("Speed", 1.0f);
        ac.Time = ja.value("Time", 0.0f);
        ac.Skin = nullptr;
    } else if (entity.HasComponent<AnimatorComponent>()) {
        entity.RemoveComponent<AnimatorComponent>();
    }
}

}
