---
title: Sprites, círculos e texto 2D
group: Componentes
order: 2
---

# Sprites, círculos e texto 2D

Os elementos 2D são desenhados **depois do mundo 3D e antes da interface**
— por isso sprites aparecem na frente de objetos 3D e atrás dos menus.

## SpriteRenderer

Um quadrado com textura ou cor sólida.

| Campo | Descrição |
|-------|-----------|
| **Color** | Cor RGBA (tinta sobre a textura; sem textura = cor sólida) |
| **Texture** | Textura (`.png`/`.jpg`, caminho relativo) |
| **Tiling** | Repetição da textura |
| **SortingLayer** | Ordenação (menor desenha atrás) |
| **FlipX / FlipY** | Inverte o sprite no espaço local |

```csharp
Entity.AddSprite("Assets/Textures/jogador.png");
Entity.SetSpriteColor(1f, 1f, 1f, 1f);
Entity.SetSpriteFlip(flipX: true, flipY: false);
Entity.SetSortingLayer(5);
```

## CircleRenderer

Círculo desenhado por **SDF** — disco cheio ou anel com borda suavizada.

| Campo | Descrição |
|-------|-----------|
| **Color** | Cor RGBA |
| **Thickness** | `1.0` = disco cheio; `<1` = anel |
| **Fade** | Suavização da borda |
| **SortingLayer** | Ordenação |

```csharp
var coin = Scene.CreateEntity("Moeda");
coin.AddSprite();
coin.SetSpriteColor(1f, 0.8f, 0.2f, 1f); // conveniente: use um sprite pequeno
```

## TextComponent

Texto 2D (HUD, pontuação, diálogo) com a **fonte embutida** JetBrains Mono
(atlas gerado via stb_truetype — nenhum arquivo de fonte externo).

| Campo | Descrição |
|-------|-----------|
| **Text** | Conteúdo; `\n` quebra linha |
| **FontSize** | Altura em pixels de tela |
| **Color** | Cor RGBA |
| **Alignment** | Esquerda / centro / direita |
| **SortingLayer** | Ordenação |

```csharp
var hud = Scene.CreateEntity("HUD");
hud.AddText("Pontos: 0", 32f);
hud.SetText("Pontos: 100");
hud.SetTextSize(48f);
hud.SetTextColor(1f, 1f, 1f, 1f);
hud.SetPosition(new Vector3(-8f, 5f, 0f));
```

## Ordenação (SortingLayer)

Camadas menores são desenhadas **atrás**. Sirva-se de camadas negativas para
fundos e positivas para elementos da frente:

```
SortingLayer -5  → fundo (desenhado primeiro)
SortingLayer  0  → jogo
SortingLayer  5  → HUD
```
