#include "kizuri/scene/EditorHistory.hpp"
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include "ComponentSerialization.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kizuri {





EntitySnapshot EntitySnapshot::Capture(Entity entity) {
    EntitySnapshot snap;
    if (entity) snap.m_Data = detail::SerializeEntityJson(entity).dump();
    return snap;
}

void EntitySnapshot::Restore(Entity entity) const {
    if (!entity || m_Data.empty()) return;
    detail::ApplyEntityStateJson(entity, json::parse(m_Data));
}





static void CollectSubtreeJson(Entity entity, json& outArray) {
    outArray.push_back(detail::SerializeEntityJson(entity));
    for (Entity child : entity.GetChildren())
        CollectSubtreeJson(child, outArray);
}

SubtreeSnapshot SubtreeSnapshot::Capture(Entity root) {
    SubtreeSnapshot snap;
    if (!root) return snap;

    json arr = json::array();
    CollectSubtreeJson(root, arr);
    snap.m_Data = arr.dump();
    return snap;
}

Entity SubtreeSnapshot::RestoreWithOriginalIds(Scene& scene) const {
    if (m_Data.empty()) return {};

    json arr = json::parse(m_Data);
    if (arr.empty()) return {};

    
    
    
    
    
    for (auto& je : arr)
        detail::DeserializeEntityJson(je, scene, je.value("ID", (uint64_t)0));

    
    
    
    
    for (auto& je : arr) {
        uint64_t parentId = je.value("Parent", (uint64_t)0);
        if (parentId == 0) continue;
        Entity child = scene.GetEntityByUUID(UUID(je.value("ID", (uint64_t)0)));
        Entity parent = scene.GetEntityByUUID(UUID(parentId));
        if (child && parent) scene.SetParent(child, parent);
    }

    return scene.GetEntityByUUID(UUID(arr.front().value("ID", (uint64_t)0)));
}





EntityEditCommand::EntityEditCommand(UUID entity, EntitySnapshot before, EntitySnapshot after)
    : m_Entity(entity), m_Before(before), m_After(after) {}

void EntityEditCommand::Undo(Scene& scene) {
    Entity e = scene.GetEntityByUUID(m_Entity);
    if (e) m_Before.Restore(e);
}

void EntityEditCommand::Redo(Scene& scene) {
    Entity e = scene.GetEntityByUUID(m_Entity);
    if (e) m_After.Restore(e);
}





CreateEntityCommand::CreateEntityCommand(Entity created)
    : m_Entity(created.GetUUID()), m_Snapshot(EntitySnapshot::Capture(created)) {}

void CreateEntityCommand::Undo(Scene& scene) {
    Entity e = scene.GetEntityByUUID(m_Entity);
    if (e) scene.DestroyEntity(e);
}

void CreateEntityCommand::Redo(Scene& scene) {
    
    
    
    Entity e = scene.CreateEntityWithUUID((uint64_t)m_Entity, "Entidade");
    m_Snapshot.Restore(e);
}





DeleteEntityCommand::DeleteEntityCommand(Entity toDelete)
    : m_Subtree(SubtreeSnapshot::Capture(toDelete)), m_Root(toDelete.GetUUID()) {}

void DeleteEntityCommand::Undo(Scene& scene) {
    m_Subtree.RestoreWithOriginalIds(scene);
}

void DeleteEntityCommand::Redo(Scene& scene) {
    Entity e = scene.GetEntityByUUID(m_Root);
    if (e) scene.DestroyEntity(e);
}





ReparentCommand::ReparentCommand(UUID child, UUID oldParent, UUID newParent)
    : m_Child(child), m_OldParent(oldParent), m_NewParent(newParent) {}

void ReparentCommand::Undo(Scene& scene) {
    Entity child = scene.GetEntityByUUID(m_Child);
    Entity oldParent = scene.GetEntityByUUID(m_OldParent);
    if (child) scene.SetParent(child, oldParent); 
}

void ReparentCommand::Redo(Scene& scene) {
    Entity child = scene.GetEntityByUUID(m_Child);
    Entity newParent = scene.GetEntityByUUID(m_NewParent);
    if (child) scene.SetParent(child, newParent);
}





void CommandHistory::Push(Ref<EditorCommand> command) {
    m_UndoStack.push_back(command);
    if (m_UndoStack.size() > kMaxHistory) m_UndoStack.erase(m_UndoStack.begin());
    m_RedoStack.clear();
}

void CommandHistory::Undo(Scene& scene) {
    if (m_UndoStack.empty()) return;
    Ref<EditorCommand> cmd = m_UndoStack.back();
    m_UndoStack.pop_back();
    cmd->Undo(scene);
    m_RedoStack.push_back(cmd);
}

void CommandHistory::Redo(Scene& scene) {
    if (m_RedoStack.empty()) return;
    Ref<EditorCommand> cmd = m_RedoStack.back();
    m_RedoStack.pop_back();
    cmd->Redo(scene);
    m_UndoStack.push_back(cmd);
}

void CommandHistory::Clear() {
    m_UndoStack.clear();
    m_RedoStack.clear();
}

} 
