---
title: Projetos, cenas e prefabs
group: Conceitos
order: 1
---

# Projetos, cenas e prefabs

Tudo na Kizuri gira em torno de três arquivos de texto legível (JSON):

| Formato | O que é |
|---------|---------|
| **`.kzproj`** | O **projeto** — modo padrão (2D/3D/Vazio) e configurações. |
| **`.kzscene`** | Uma **cena** — a lista de entidades e componentes. |
| **`.kzprefab`** | Uma **entidade reutilizável** — instanciada quantas vezes quiser. |

## Projeto (.kzproj)

É a raiz de tudo. Ao criar um projeto você escolhe o **modo**:

- **2D** — cenas novas nascem com câmera ortográfica + sprites + física 2D;
- **3D** — cenas novas nascem com câmera de perspectiva + mesh;
- **Vazio** — só uma câmera.

O modo não é uma camisa de força: você pode misturar 2D e 3D na mesma cena
(cenas **2.5D**). Ele só define o que vem pronto no primeiro frame.

## Cena (.kzscene)

Uma cena é uma lista de entidades, cada uma com seus componentes. O arquivo é
JSON legível — dá para editar no bloco de notas e versionar no git:

```json
{
  "entities": [
    {
      "id": "9f1c…",
      "tag": "Jogador",
      "parent": null,
      "transform": { "t": [0, 0, 0], "r": [0, 0, 0], "s": [1, 1, 1] },
      "components": [ … ]
    }
  ]
}
```

### Salvar e abrir

- **Arquivo → Salvar Cena** (`Ctrl+S`) salva a cena atual.
- **Arquivo → Salvar Cena Como…** salva em outro caminho.
- **Arquivo → Abrir Cena…** / **Cena → Nova Cena** troca a cena.
- No jogo, `Scene.Load("Assets/Fase2.kzscene")` troca de cena em runtime.

## Prefab (.kzprefab)

Uma prefab é uma entidade (com toda a subárvore de filhos) salva como arquivo.
Serve como "molde": o mesmo inimigo, o mesmo projétil, o mesmo item.

Em runtime:

```csharp
// instancia na posição dada (a física e o OnCreate rodam de verdade)
var inimigo = Scene.InstantiatePrefab("Assets/Inimigo.kzprefab", new Vector3(3f, 0f, 0f));

// com rotação (euler, radianos)
var tiro = Scene.InstantiatePrefab("Assets/Tiro.kzprefab", pos, rot);
```

::: info
**Caminhos relativos.** Todos os caminhos de asset (texturas, meshes, clips,
prefabs, cenas) são resolvidos **relativos ao projeto**. Ex.:
`Assets/Models/Cube.glb`. Também são aceitos os pseudo-caminhos `builtin:*` e
`kzres://*` (veja [Recursos embutidos](recursos-embutidos.html)).
:::
