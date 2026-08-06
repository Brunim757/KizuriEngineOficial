---
title: Animação 3D
group: Componentes
order: 13
---

# Animação 3D

A animação 3D é **esquelética (skinning)** via glTF: modelos `.glb`/`.gltf`
com juntas e clips de animação rodam direto no editor e no jogo.

## Como usar

1. Adicione um **MeshRenderer** com um `.glb` animado (ex.: o Fox da demo).
2. Adicione o componente **Animador** (mesmo caminho do modelo).
3. No painel **Animator**, escolha o **clip**, play/pause, loop, velocidade e
   arraste a **linha do tempo** para qualquer pose.

## Componente Animator

- **Clip** — nome da animação no arquivo
- **Playing / Loop / Speed**
- **Time** — posição atual (segundos)

## No script

```csharp
Entity.AddAnimator("Assets/Models/Fox.glb");
Entity.PlayAnimation("Survey");
Entity.SetAnimationSpeed(0.5f);
Entity.SetAnimationLoop(true);
```

## Dicas

- A **sombra** acompanha a pose animada (o passe de sombra aplica o
  skinning) — o Fox projeta sombra na pose atual.
- O painel **Animator** (menu Janelas) tem o controle completo.

::: dica
O Content Pack traz o **Fox** (skin + animações) e o **DamagedHelmet**
(material PBR) — perfeitos para testar skinning e materiais.
:::
