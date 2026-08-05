---
title: Animação esquelética
group: Componentes
order: 8
---

# Animação esquelética (skinning)

Toca **animações de um `.glb`/`.gltf`** em uma malha com skeleton — personagens
e criaturas modelados em Blender etc. funcionam direto.

## AnimatorComponent

| Campo | Descrição |
|-------|-----------|
| **MeshPath** | Fonte `.glb`/`.gltf` (o mesmo arquivo do MeshRenderer) |
| **ClipName** | Animação atual (nome da `animation` no arquivo) |
| **Playing** | Toca/pausa |
| **Loop** | Repete ao fim |
| **Speed** | Velocidade de reprodução |
| **Time** | Posição na animação (segundos) |

## Configurando

1. Adicione **MeshRenderer** apontando para `Assets/Models/Personagem.glb`;
2. Adicione **Animator** com o mesmo caminho;
3. Escolha o **ClipName** (ex.: `"Run"`, `"Idle"` — os nomes vêm do arquivo).

O renderer aplica a **skin** (matrizes de junta) no vertex shader — e também
no **shader de sombra**, então a sombra "anda" junto com o personagem.

## Em C#

```csharp
Entity.AddMeshRenderer("Assets/Models/Fox.glb");
Entity.AddAnimator("Assets/Models/Fox.glb");
if (Entity.PlayAnimation("Survey")) { }
Entity.SetAnimationSpeed(1.5f);
Entity.SetAnimationLoop(true);
Entity.SetAnimationPlaying(true);
Entity.SetAnimationTime(2f);
var t = Entity.AnimationTime;
```

## Sobre a skin

- A **skin é reparseada** ao abrir a cena (não é serializada);
- `SkinData` suporta juntas, pesos por vértice e múltiplos **clips**;
- O Fox de demonstração tem 19 juntas e ~8 animações — experimente na
  [demonstração 3D](interface.html) (menu **Cena**).

::: info
**Clip vazio = pose de repouso.** Deixe `ClipName` vazio para mostrar a malha
na pose do arquivo sem animar.
:::
