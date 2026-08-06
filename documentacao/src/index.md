---
title: Bem-vindo
group: Introdução
order: 1
---

# Kizuri Engine

A **Kizuri Engine** é um motor de jogos 2D e 3D completo — criado para você
fazer o jogo inteiro dentro do editor, **sem mexer em C++**.

- **Motor em C++**, 100% **OpenGL 3.3** — roda em praticamente qualquer máquina, sem drivers especiais
- **Jogo em C#** (`Kizuri.Scripting`) — você escreve a lógica em uma linguagem simples e moderna
- **Editor visual completo** com 10+ painéis (Hierarquia, Inspetor, Viewport, Game View, Profiler, Content Browser, Console, Material Editor, Animator, Project Settings)
- **Gráficos de respeito** no 3.3: PBR, reflexos por raio, iluminação global, céu físico, sombras suaves, nuvens volumétricas, DOF, motion blur...
- **Física 2D e 3D** (Box2D e Bullet), áudio, partículas, tilemap, UI e exportação do jogo como executável standalone

## Para quem é esta documentação

- **Comece aqui** → [Instalação](instalacao.html) e [Seu primeiro projeto](primeiro-projeto.html)
- **Conheça o editor** → seção [Editor](interface.html)
- **Aprenda a criar jogos** → seção [Componentes](transform.html)
- **Escreva scripts** → seção [Scripting C#](scripting.html)
- **Capriche nos gráficos** → seção [Gráficos](qualidade.html)

## O ciclo de trabalho

1. **Crie um projeto** no editor (2D, 3D ou vazio).
2. **Monte a cena**: adicione entidades, meshes, luzes, física, áudio, UI...
3. **Escreva scripts C#** e aperte **Play** — o editor compila e roda o jogo.
4. **Exporte o jogo**: um executável standalone com seus assets embutidos.

::: dica Quer ver tudo funcionando?
Abra o menu **Arquivo → Cena de Demonstração 3D** no editor — uma cena pronta com esqueleto animado, material PBR, reflexos e céu físico.
:::
