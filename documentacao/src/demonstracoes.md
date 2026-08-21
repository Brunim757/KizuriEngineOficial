---
title: Demonstrações
group: Editor
order: 9
---

# Demonstrações

O editor vem com **18 cenas de demonstração** prontas — uma para cada
recurso da engine. Elas mostram como montar cada sistema na prática e
servem de ponto de partida pra copiar configurações pro seu jogo.

**Menu:** Cena → Demonstrações

## 2D

| Demo | O que mostra |
|------|--------------|
| **Sprites, física e UI** | Sprite colorido, física Box2D (caixas que caem), moedas circulares, texto, UI Canvas com botão |
| **Tilemap** | Mapa 24×14 tiles com atlas procedural, tiles sólidos e colisão |
| **Animação 2D** | Sprite sheet procedural (bola) — loop a diferentes FPS e one-shot |
| **Câmera Follow** | CameraFollowComponent + TimelineComponent em alvo que se move pelo mapa |

## 3D

| Demo | O que mostra |
|------|--------------|
| **PBR, esqueleto e céu** | Malha Fox animada, capacete PBR, torus metálico, céu HDRI |
| **Física 3D** | Torre de caixas e esferas caindo no chão (Rigidbody3D + BoxCollider3D) |
| **Luzes** | Luzes pontuais (RGB), spots com sombras, emisivo — cena escura |
| **Partículas** | Fogo (additive), fumaça (alpha blend), faíscas (fountain) |
| **Terreno** | TerrainComponent heightmap procedural + água espelhada |
| **Timeline** | Plataforma que anda em circuito + torus girando via KeyframeComponent |
| **LOD** | Esferas com LODComponent — afaste pra ver redução de triângulos |
| **Efeitos** | Decal projetado, foliage instanciada (60 árvores), occluder |

## Hibridos

| Demo | O que mostra |
|------|--------------|
| **2.5D** | Mundo 3D (pilares, chão) + camada 2D (sprites + círculos) + HUD (texto + UI canvas) |

## IA, Rede e Mundo

| Demo | O que mostra |
|------|--------------|
| **IA** | NavGrid + patrulha/perseguição com EnemyAI e NavAgent |
| **Rede** | Multiplayer local (Host/Connect), sincronização de transform via rede |
| **Chunk World** | Mundo em pedaços: entidades com ChunkEntityComponent numa grade 3×3 |

## Jogo

| Demo | O que mostra |
|------|--------------|
| **Jogo completo** | Mini-jogo: colete 8 moedas fugindo dos inimigos, placar, WASD |

::: dica
As demos são o melhor atalho pra aprender: abra uma, entre no Play,
depois inspecione cada entidade pra ver como os componentes estão
configurados.
:::

::: dica
Todas as demos usam **texturas procedurais** (geradas em tempo de
criação) — não precisam do Content Pack pra funcionar.
:::

## Como as demos são organizadas

Cada demo é gerada por código dentro do editor (arquivo `DemoScenes.cpp`)
— não são arquivos `.kzscene` salvos. Isso garante que a demo sempre usa
os valores padrão atualizados da engine, sem depender de arquivos antigos.

Adicionar uma demo nova = uma função nova em `DemoScenes.cpp` + um item
no submenu Cena → Demonstrações.

## Fluxo recomendado

1. Abra a demo do recurso que você quer aprender (ex.: IA).
2. Aperte **Play** e interaja.
3. Pare o Play e explore o Inspetor das entidades envolvidas.
4. Copie os componentes pro seu próprio jogo.