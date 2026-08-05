---
title: MeshRenderer e materiais
group: Componentes
order: 5
---

# MeshRenderer e materiais

Desenha uma **mesh 3D** com um **material PBR**.

## MeshRendererComponent

| Campo | Descrição |
|-------|-----------|
| **MeshSource** | `builtin:cube\|plane\|…` ou caminho de `.obj` / `.glb` / `.gltf` |
| **Material** | Cor base, metálico, rugosidade, emissão e mapas de textura |

## Meshes builtin

Todas geradas em código — funcionam sem nenhum arquivo:

```
builtin:cube · builtin:plane · builtin:sphere · builtin:cylinder
builtin:cone · builtin:capsule · builtin:torus
```

## Importando modelos

- **`.obj`** — geometria + material serializável;
- **`.glb` / `.gltf`** — geometria, **animações esqueléticas**, e o material
  PBR **extraído automaticamente** (fatores + texturas embutidas no arquivo).

Para modelos animados, veja [Animação esquelética](animacao-3d.html).

## Material PBR

| Campo | Descrição |
|-------|-----------|
| **Albedo** | Cor base |
| **Metallic** | 0 = dielétrico, 1 = metal |
| **Roughness** | 0 = espelhado, 1 = rugoso |
| **AO** | Oclusão ambiente (afeta só o IBL) |
| **Emissive / EmissiveStrength** | Emissão (alimenta o **bloom**) |
| **AlbedoMap** | Textura de cor |
| **NormalMap** | Normal mapping (tangent-space, sem tangentes na mesh) |
| **MetallicRoughnessMap** | Canal **G = rugosidade**, **B = metálico** (convenção glTF) |
| **EmissiveMap** | Textura emissiva |

```csharp
Entity.AddMeshRenderer("builtin:torus");
Entity.SetMaterial(0.75f, 0.55f, 0.2f, metallic: 1f, roughness: 0.25f);
Entity.SetMaterialAlbedoMap("Assets/Textures/metal.png");
Entity.SetMaterialEmissive(0.1f, 0.6f, 1f, strength: 6f); // brilha no bloom
```

## No editor

Arraste um `.obj`/`.glb` do [Content Browser](content-browser.html) para o
viewport — a entidade nasce com a mesh e o material já extraídos. Use os
campos do Inspetor para afinar o material, e o material PBR atualiza no
viewport em tempo real.

::: ok
**Texturas nunca se perdem.** O material extraído de um `.glb` guarda os
caminhos serializáveis — ao reabrir a cena, os mapas são reextraídos do
arquivo fonte automaticamente.
:::
