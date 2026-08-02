#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/UUID.hpp"
#include "kizuri/ecs/Entity.hpp"
#include <string>
#include <vector>

namespace kizuri {

class Scene;

// ---------------------------------------------------------------------
// Snapshots opacos: guardam estado serializado sem expor nlohmann::json
// na API pública da engine (mesma decisão de SceneSerializer/Prefab — ver
// src/scene/ComponentSerialization.hpp, que é quem faz o trabalho real).
// ---------------------------------------------------------------------

// Estado dos componentes de UMA entidade (sem filhos, sem ID/Parent — só
// os dados editáveis: transform, sprite, câmera, colisores etc).
class EntitySnapshot {
public:
    static EntitySnapshot Capture(Entity entity);
    void Restore(Entity entity) const;

    // Usado pra evitar empilhar um comando de undo quando um widget ficou
    // ativo (ex: cliquei e soltei sem arrastar) mas nada mudou de fato.
    bool DiffersFrom(const EntitySnapshot& other) const { return m_Data != other.m_Data; }

private:
    std::string m_Data;
};

// Estado de uma entidade E de toda a subárvore de filhos dela, preservando
// os UUIDs originais — diferente de uma Prefab (que gera UUIDs novos a
// cada instanciação), aqui o objetivo é trazer de volta exatamente o mesmo
// objeto que existia antes, pra desfazer um "Deletar Entidade".
class SubtreeSnapshot {
public:
    static SubtreeSnapshot Capture(Entity root);
    Entity RestoreWithOriginalIds(Scene& scene) const;

private:
    std::string m_Data;
};

// ---------------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------------

class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual void Undo(Scene& scene) = 0;
    virtual void Redo(Scene& scene) = 0;
};

// Cobre qualquer edição de propriedade de uma entidade que já existe:
// mover no gizmo, arrastar uma cor, marcar um checkbox, adicionar ou
// remover componente. Não precisa de uma classe de comando por tipo de
// edição — o "antes" e "depois" já são o snapshot inteiro dos componentes.
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

// ---------------------------------------------------------------------
// Histórico: pilha de undo + pilha de redo. Empilhar um comando novo
// sempre limpa o redo — comportamento padrão de qualquer editor, não dá
// pra "refazer" uma ramificação que deixou de existir.
// ---------------------------------------------------------------------
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

} // namespace kizuri
