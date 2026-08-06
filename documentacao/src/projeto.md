---
title: Gerenciamento de projetos
group: Introdução
order: 4
---

# Gerenciamento de projetos

Um **projeto** é a pasta de um jogo: cenas, assets, scripts e o arquivo do
projeto (`.kzproj`).

## Hub

O editor abre no **Hub** (tela inicial), onde você:

- **Cria um novo projeto** (2D, 3D ou vazio)
- **Abre um recente** (a lista fica salva)
- **Abre um projeto** pelo caminho

## Estrutura do projeto

```
MeuJogo/
  MeuJogo.kzproj     ← arquivo do projeto (config + cena inicial)
  Assets/            ← cenas, prefabs, modelos, texturas, áudios
  Source/            ← scripts C# do jogo
```

## Abrir e salvar cenas

- **Arquivo → Salvar Cena** / **Salvar Cena Como...** (`.kzscene`)
- **Arquivo → Abrir Cena...** — cenas grandes carregam de forma
  **incremental** (sem travar o editor, com barra de progresso)
- **Arquivo → Definir cena como inicial** — a cena que o jogo carrega ao
  abrir/exportar

## Prefabs

No **Inspetor** (ou hierarquia), **botão direito → Salvar como Prefab**
gera um `.kzprefab` reutilizável. Arraste o prefab para o viewport ou use
`Scene.InstantiatePrefab` no código.

::: dica
Troque de projeto quando quiser: **Arquivo → Voltar ao Início** leva você
de volta ao Hub sem perder nada.
:::
