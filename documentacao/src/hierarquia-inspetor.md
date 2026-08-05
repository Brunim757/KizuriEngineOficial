---
title: Hierarquia e Inspetor
group: Editor
order: 3
---

# Hierarquia e Inspetor

## Hierarquia

O painel **Hierarquia** lista todas as entidades da cena em árvore. Você pode:

- **Selecionar** (clicando) — o Inspetor mostra a entidade;
- **Parentar** (arrastar sobre outra) e **destacar**;
- **Renomear** — duplo clique no nome;
- **Buscar** — o campo de filtro filtra a árvore pelo nome;
- **Duplicar** (`Ctrl+D`) e **Excluir** (`Del`).

Novas entidades podem ser criadas pelo botão no painel, pelo menu
**Editar**, ou em runtime via `Scene.CreateEntity`.

## Inspetor

O Inspetor mostra a entidade selecionada e seus componentes. Para cada
componente há uma seção colapsável com seus campos:

- **Transform** — posição, rotação (graus), escala;
- **Componentes de render** — cor, textura, material, ordenação;
- **Física** — tipo de corpo, massa, colliders;
- **Scripts** — botão para adicionar um script registrado no GameModule.

### Adicionar um componente

1. Selecione a entidade.
2. No Inspetor, clique em **Adicionar** e escolha o componente (ou digite o
   nome para buscar).
3. Ajuste as propriedades.

::: info
A maioria dos componentes tem **gizmo no viewport** quando selecionado —
colliders desenham o contorno, luzes desenham a direção/alcance, partículas
mostram o cone de emissão.
:::

## Estados de edição

O editor tem dois estados:

- **Edit** — você monta a cena; física/scripts **não** rodam;
- **Play** — cópia isolada da cena com física, scripts, partículas e áudio
  rodando de verdade.

Os dois compartilham o mesmo Inspetor: durante o Play você vê o estado
runtime dos componentes (posições, velocidades, tempos).
