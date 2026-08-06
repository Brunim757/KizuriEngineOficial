---
title: Malhas 3D
group: Componentes
order: 3
---

# Malhas 3D

O componente **MeshRenderer** desenha uma malha 3D com um material PBR.

## Malhas prontas

No Inspetor, o campo **MeshSource** aceita:

- `builtin:cube`, `builtin:plane`, `builtin:sphere`
- `builtin:cylinder`, `builtin:cone`, `builtin:capsule`, `builtin:torus`
- um arquivo `.obj`, `.glb` ou `.gltf` do seu projeto

## Material

O material PBR controla:

- **Albedo** — cor base (e textura)
- **Metallic** — 0 = plástico, 1 = metal
- **Roughness** — 0 = polido, 1 = áspero
- **AO** — oclusão ambiente local
- **Emissive** — brilho próprio (alimenta o bloom)
- **Mapas** — albedo, normais, metallic/roughness, emissivo e **altura (POM)**
- **Reflexão planar** — transforma a superfície em espelho

::: dica
Ao arrastar um `.glb` para o viewport, a engine importa a malha **e** o
material PBR (fatores + texturas) automaticamente.
:::

## POM — relevo de superfície

Com um **mapa de altura**, a superfície ganha relevo real que acompanha o
ângulo da câmera (parallax occlusion mapping) — paredes de pedra, tijolos,
chão com desnível.

Veja mais em [Iluminação](iluminacao.html) e [Reflexos](reflexos.html).
