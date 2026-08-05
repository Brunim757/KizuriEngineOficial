---
title: Pipeline 3D
group: Renderização
order: 1
---

# Pipeline 3D

O renderer 3D é **PBR forward** com pós-processamento HDR. O nível de efeitos
depende da versão do **OpenGL** detectada no contexto real da sua máquina.

## Versão-gated por hardware

| Recurso | OpenGL 3.3 (estável) | OpenGL 4.0+ (ultra) |
|---------|----------------------|---------------------|
| PBR + IBL + CSM + bloom + SSAO | ✔ | ✔ |
| Suavização de sombra | **PCF** | **PCSS** (área real) |
| Luz pontual | ✔ (sem sombra) | ✔ **com cubemap de sombra** |
| MSAA padrão | 4x | 8x |

A versão GLSL é detectada do **driver real** (`GL_SHADING_LANGUAGE_VERSION`),
travada pelo teto do contexto core criado. Sem chutes: se o driver diz 3.3,
os shaders são 330.

::: ok
**3.3 não é "modo pobre".** É o caminho **estável comprovado** — PBR, IBL,
CSM, bloom, SSAO, partículas e skinning rodam todos em 3.3. Só o que exige
loop dinâmico (PCSS, sombra de ponto) é reservado para 4.0+.
:::

## Recursos do pipeline

- **PBR Cook-Torrance** — GGX + Smith + Fresnel-Schlick, com metallic/roughness;
- **IBL** — céu (procedural ou HDRI) → cubemap → irradiância difusa +
  pré-filtro especular GGX;
- **Multi-luz** — até **16 luzes** por frame (Direcional/Ponto/Spot);
- **Sombras em cascata (CSM)** — 3 faixas que acompanham a câmera (nitidez
  perto, cobertura longe);
- **PCSS** — penumbra real (proporcional à distância do oclusor);
- **Normal mapping** — por derivada de tela, sem tangentes na mesh;
- **Partículas GPU-instanced** com billboarding;
- **Skinning** no vertex shader **e no shader de sombra** (a sombra anda junto);
- **Bloom** — bright-pass + blur gaussiano (iterações configuráveis);
- **Tonemapping** — ACES / Reinhard / Filmic;
- **SSAO** — oclusão em espaço de tela;
- **Névoa exponencial** — atmosfera por distância;
- **Pós-cinema** — vinheta, aberração cromática e grão de filme animado.

## Céu

| Fonte | Quando |
|-------|--------|
| **HDRI** (Content Pack) | preferido se presente (`.hdr` equirectangular) |
| **Embutido** `kzres://skies/sky_gradient.hdr` | padrão — sempre disponível |
| **Procedural** | fallback final (céu atmosférico escuro + sol) |

## Materiais e luzes

- Veja [MeshRenderer e materiais](meshes-3d.html) e [Luzes](luzes.html).

## Ordem de renderização da cena

```
1. Passe 3D  (câmera de perspectiva)
2. Passe 2D  (câmera ortográfica) — sprites/texto/tilemap
3. Passe UI  (canvas em espaço de tela)
```

Cada passe só roda se a cena tem uma câmera primária do tipo correspondente —
é isso que permite jogos 100% 2D, 100% 3D ou **2.5D** (as duas).
