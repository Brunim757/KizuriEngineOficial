---
title: Content Browser
group: Editor
order: 4
---

# Content Browser

O **Content Browser** mostra os arquivos da pasta atual do projeto. É a sua
"caixa de assets".

## Drag & drop

Arraste um asset direto para o **viewport** e a entidade nasce pronta:

| Asset | O que cria |
|-------|------------|
| `.obj` / `.glb` / `.gltf` | Entidade com `MeshRenderer` + material extraído do modelo |
| `.png` / `.jpg` | Entidade com `SpriteRenderer` usando a textura |
| `.wav` / `.ogg` | Entidade com `AudioSource` |
| `.kzprefab` | Entidade instanciada da prefab |

## Ícones por tipo

Cada tipo de arquivo tem um ícone colorido para reconhecimento rápido:
modelos, texturas, áudio, prefabs, cenas.

## Renomear e organizar

- **Renomeie** assets e entidades diretamente (duplo clique no nome).
- Crie pastas para organizar (`Assets/Models`, `Assets/Sounds`, …).

## Campos de arquivo

Nos componentes, os campos de caminho têm um botão **…** que abre o diálogo
nativo de seleção (Windows). No Linux/macOS, digite o caminho manualmente.

## Caminhos

Todos os caminhos são **relativos ao projeto**:

```
Assets/Models/Cube.glb
Assets/Textures/piso.png
Assets/Sounds/tiro.wav
```

Também são aceitos:

- `builtin:cube|plane|sphere|cylinder|cone|capsule|torus` — meshes geradas;
- `kzres://…` — [recursos embutidos](recursos-embutidos.html).
