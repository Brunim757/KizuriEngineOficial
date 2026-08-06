---
title: Content Browser
group: Editor
order: 4
---

# Content Browser

O painel **Content Browser** mostra os arquivos do projeto: modelos, texturas,
áudios, cenas, prefabs e scripts.

## Atalhos de uso

- **Clique** seleciona o arquivo; **duplo clique** abre (cena/prefab) ou mostra o preview
- **Botão direito** abre o menu de contexto: Novo (Pasta, Script C#, Cena, Prefab), Renomear, Excluir
- **Arraste um asset para o viewport** para criar a entidade:
  - `.obj/.glb/.gltf` → **MeshRenderer**
  - imagem (`.png/.jpg/...`) → **SpriteRenderer**
- **Arraste um asset para um slot do Inspetor** (ex.: campo de textura) para aplicá-lo

## Estrutura típica de um projeto

```
MeuJogo/
  MeuJogo.kzproj     ← arquivo do projeto
  Assets/            ← seus arquivos (modelos, texturas, scripts, cenas)
  Source/            ← código C# do jogo (compilado no Play)
```

## Previews

O Content Browser mostra **miniaturas reais** de imagens (com cache por
caminho). Pastas gigantes não travam o editor: as miniaturas são geradas com
orçamento por frame.

## Importação

A engine importa diretamente:

- **Modelos**: `.obj`, `.glb`, `.gltf` (o glTF traz materiais PBR, skins e animações)
- **Imagens**: `.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`
- **Céus**: `.hdr`, `.exr` (equirectangular)
- **Áudio**: `.wav`, `.ogg`, `.mp3`
- **Cenas**: `.kzscene` · **Prefabs**: `.kzprefab`

::: dica
Assets embutidos da engine usam o prefixo `kzres://` (ex.: `kzres://skies/sky_gradient.hdr`).
:::
