---
title: Câmera
group: Componentes
order: 10
---

# Câmera

Define **o que** e **como** a cena é renderizada. Uma cena pode ter várias
câmeras; a marcada como **Primary** é a que renderiza.

## CameraComponent

| Campo | Descrição |
|-------|-----------|
| **Type** | `Orthographic2D` ou `Perspective3D` |
| **OrthoSize** | Meia-altura da projeção ortográfica (unidades de mundo) |
| **PerspectiveFOV** | Campo de visão vertical (graus) |
| **NearClip / FarClip** | Plano próximo / distante |
| **Primary** | É a câmera que renderiza? |
| **FixedAspectRatio** | Trava a razão de aspecto |

## Modos

- **Orthographic2D** — para jogos 2D / HUD. O tamanho do mundo visto depende
  do `OrthoSize` e da resolução da janela (a largura se ajusta pelo aspecto).
- **Perspective3D** — para jogos 3D. `FOV` maior = visão mais aberta.

## Câmera primária

- Se **nenhuma** câmera está marcada como primary, a primeira da cena é usada;
- A **primeira câmera** com primary decide o passe: se for ortográfica, o
  passe 2D roda; se perspectiva, o passe 3D roda. Cenas **2.5D** têm as duas
  (3D de fundo + 2D na frente + UI por cima).

## No editor vs. no Play

| Estado | Quem renderiza |
|--------|----------------|
| Edição | A **câmera de navegação** do editor (livre) |
| **Play / exportado** | A **câmera da própria cena** |

::: info
**O Play é o que o jogo será.** Configure a câmera da cena — a posição da
câmera de edição não afeta o jogo.
:::

## Em C#

```csharp
var cam = Scene.CreateEntity("Câmera");
cam.AddCamera(perspective3D: true);
cam.SetCamera(fovDeg: 55f, nearClip: 0.01f, farClip: 1000f);
cam.SetPosition(new Vector3(0f, 2f, 6f));

var principal = Scene.GetPrimaryCamera(); // pega a câmera primária
```
