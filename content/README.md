# Content Pack (Kizuri)

Pasta de conteúdo de exemplo distribuída junto da engine/artifacts. É aqui
que o "peso" profissional da engine vai morar: modelos, texturas PBR, HDRIs
de céu, áudios e prefabs de demonstração — tudo consumido pelos exemplos e
pelo editor (arraste do Content Browser pro viewport/Inspector).

## Estrutura

```
content/
  models/   modelos glTF/glb (o padrão da indústria)
  textures/ texturas PBR (albedo/normal/metallic/roughness/AO)
  skies/    HDRIs equirectangulares (.hdr) pro ambiente IBL
  audio/    efeitos sonoros e músicas
  prefabs/  prefabs .kzprefab reutilizáveis
```

## Regras

- Assets aqui são CC0/public domain (ou com licença incluída ao lado). Nada
  de asset proprietário.
- Arquivos pequenos vão commitados no git; packs grandes entram via CI
  (download pra `dist/content/` no artifact) pra não inchar o repositório.

## Atual

- `models/Cube.glb` — cubo PBR (gerado, ~1.8KB): primeiro asset real da
  engine, prova o pipeline glTF/glb (cgltf) de ponta a ponta.
