---
title: Visão geral
group: Introdução
order: 1
---

# Kizuri Engine

A **Kizuri Engine** é uma engine de jogos **2D + 3D** escrita em C++20, com
editor visual integrado e programação em **C#**. Você monta a cena no editor,
escreve a lógica em C# e exporta um executável standalone — no mesmo espírito
de Unity e Godot, mas com uma arquitetura enxuta e privada.

## O que a engine oferece

| Área | Recursos |
|------|----------|
| **Renderer 3D** | PBR (Cook-Torrance), IBL, CSM, PCSS, sombra de luz pontual, bloom, tonemapping (ACES/Reinhard/Filmic), SSAO, névoa, partículas GPU, skinning, até 16 luzes |
| **Renderer 2D** | Sprites, círculos (SDF), texto com fonte embutida, tilemaps, animação por frames, ordenação por camadas |
| **UI** | Canvas + Rect/Button/Text em espaço de tela, com hover/clique detectáveis por script |
| **Física** | Box2D (2D) e Bullet3 (3D) — corpos, colliders, forças, torque, raycasts, overlaps, callbacks de colisão |
| **Áudio** | miniaudio — sources posicionais 3D, one-shots, volume mestre |
| **Scripting** | Assembly C# `Kizuri.Scripting` conversando com o motor só por um ABI C |
| **Conteúdo** | `kzres://` — assets padrão **embutidos no executável**; Content Pack opcional |
| **Exportação** | Executável standalone com cena inicial + assembly C# |

## Arquitetura em uma frase

> O motor C++ é 100% privado. O jogo é um assembly **C#** que conversa com a
> engine por um **ABI C** (`kz_*`) — nenhum header ou dependência interna é
> exposta ao usuário.

## Modelo de trabalho

1. **Editor** — você constrói a cena visualmente: entidades, componentes,
   luzes, física, UI. Aperte **Play** para testar com física e scripts reais.
2. **C#** — a lógica do jogo vive em classes que herdam de `Script`, com
   `OnCreate`, `OnUpdate`, `OnCollisionBegin/End`. O editor recompila o C# no
   Play automaticamente.
3. **Exportar** — `Arquivo → Exportar Jogo…` gera um executável standalone
   que abre a cena inicial e roda o assembly.

## Self-contained

Os assets usados como **padrão** (modelos da demonstração 3D — Fox animado e
capacete PBR —, céu HDRI, primitivas) são **compilados dentro do executável**,
acessíveis pelo esquema `kzres://`. Apagar um arquivo do disco **não quebra
nenhuma função padrão**: a prioridade é sempre embutido → Content Pack →
builtins procedurais.

::: info
**"Pesada" por features, não por pacote.** A engine é grande por ter muitos
recursos — não por depender de um monte de conteúdo solto que se apaga.
:::

## Onde começar

- **[Instalação e build](instalacao.html)** — requisitos e compilação.
- **[Seu primeiro projeto](primeiro-projeto.html)** — do zero ao Play.
- **[Interface do editor](interface.html)** — conheça os painéis.
- **[Scripting C#](scripting.html)** — a API de gameplay.
