# Content Pack (Kizuri)

Pasta de conteúdo de exemplo distribuída junto da engine/artifacts. É aqui
que o "peso" profissional da engine mora: modelos, texturas PBR, HDRIs de
céu, áudios e prefabs de demonstração — tudo consumido pelos exemplos e pelo
editor (arraste do Content Browser pro viewport/Inspector).

## Estrutura

```
content/
  models/    modelos glTF/glb CC0 (o padrão da indústria)
  textures/  texturas PBR CC0 da ambientCG (albedo/normal/roughness/metalness)
  skies/     HDRIs equirectangulares CC0 (Poly Haven) pro ambiente IBL
  audio/     efeitos sonoros e músicas (futuro)
  prefabs/   prefabs .kzprefab reutilizáveis (futuro)
```

## Regras

- Assets CC0/public domain. Nada de asset proprietário.
- O pack é baixado na CI (job `package` do .github/workflows/build.yml) pra
  dentro de `dist/content/` do artifact — o zip cresce junto. Arquivos
  pequenos podem ser commitados (ex.: `models/Cube.glb`).
- Tudo best-effort: um download que falhar só omite aquele arquivo.

## Usando

- **Modelos** (`models/*.glb`): arraste pro viewport (cria MeshRenderer com o
  material PBR extraído do próprio arquivo) ou pro slot "Malha" do Inspetor.
  Os esqueléticos (Fox, BrainStem, CesiumMan, RiggedFigure, BoxAnimated)
  ganham um `AnimatorComponent` — veja o painel "Animador (skinning)".
- **HDRIs** (`skies/*.hdr`): Arquivo > Configurações Gráficas > "HDRI do céu"
  aponta pro arquivo; o bake do IBL (irradiância + pré-filtro) usa a imagem.
- **Texturas PBR** (`textures/<Material>_2K/`): albedo/normal/roughness pro
  material de qualquer MeshRenderer (slots do Inspetor aceitam drop).

## Conteúdo embutido (`kzres://`)

Uma seleção de assets vive DENTRO do executável/dll, sem arquivo no disco.
Caminhos `kzres://<nome>` funcionam em qualquer slot de asset (mesh, textura,
skin, HDRI). Atual: `models/Cube.glb` e `skies/sky_gradient.hdr`. O
registro é gerado no build por `cmake/EmbedContent.cmake` a partir de
`engine/resources/embedded_content/` — adicione arquivos lá pra embutir mais.

## Atual (gerado na última CI)

- Modelos: Fox, BoxAnimated, BrainStem, CesiumMan, RiggedFigure,
  DamagedHelmet, Avocado, BoomBox, Duck, CesiumMilkTruck, SimpleMaterials
- HDRIs: qwantani_puresky (1k/4k), kloofendal (1k/4k), venice_sunset (1k/4k),
  kloppenheim_06, spruit_sunrise, the_sky_is_on_fire, studio_small_03
- Texturas PBR (2K): Wood049, Metal032, Bricks072, Concrete034, Planks016,
  Ground037 (só no artifact Ubuntu — a etapa precisa de unzip)
