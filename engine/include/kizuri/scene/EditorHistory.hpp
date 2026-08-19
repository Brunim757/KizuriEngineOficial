#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/UUID.hpp"
#include "kizuri/ecs/Entity.hpp"
#include <string>
#include <vector>

namespace kizuri {

class Scene;









class EntitySnapshot {
public:
    static EntitySnapshot Capture(Entity entity);
    void Restore(Entity entity) const;

    
    
    bool DiffersFrom(const EntitySnapshot& other) const { return m_Data != other.m_Data; }

private:
    std::string m_Data;
};





class SubtreeSnapshot {
public:
    static SubtreeSnapshot Capture(Entity root);
    Entity RestoreWithOriginalIds(Scene& scene) const;

private:
    std::string m_Data;
};





class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual void Undo(Scene& scene) = 0;
    virtual void Redo(Scene& scene) = 0;
};





class EntityEditCommand : public EditorCommand {
public:
    EntityEditCommand(UUID entity, EntitySnapshot before, EntitySnapshot after);
    void Undo(Scene& scene) override;
    void Redo(Scene& scene) override;

private:
    UUID m_Entity;
    EntitySnapshot m_Before, m_After;
};

class CreateEntityCommand : public EditorCommand {
public:
    explicit CreateEntityCommand(Entity created);
    void Undo(Scene& scene) override;
    void Redo(Scene& scene) override;

private:
    UUID m_Entity;
    EntitySnapshot m_Snapshot;
};

class DeleteEntityCommand : public EditorCommand {
public:
    explicit DeleteEntityCommand(Entity toDelete);
    void Undo(Scene& scene) override;
    void Redo(Scene& scene) override;

private:
    SubtreeSnapshot m_Subtree;
    UUID m_Root;
};

class ReparentCommand : public EditorCommand {
public:
    ReparentCommand(UUID child, UUID oldParent, UUID newParent);
    void Undo(Scene& scene) override;
    void Redo(Scene& scene) override;

private:
    UUID m_Child, m_OldParent, m_NewParent;
};






class CommandHistory {
public:
    void Push(Ref<EditorCommand> command);
    void Undo(Scene& scene);
    void Redo(Scene& scene);
    bool CanUndo() const { return !m_UndoStack.empty(); }
    bool CanRedo() const { return !m_RedoStack.empty(); }
    void Clear();

private:
    static constexpr size_t kMaxHistory = 200;
    std::vector<Ref<EditorCommand>> m_UndoStack;
    std::vector<Ref<EditorCommand>> m_RedoStack;
};

} 
