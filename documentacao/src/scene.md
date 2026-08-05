---
title: Scene — referência
group: Scripting C#
order: 4
---

# Scene — referência

`Scene` dá acesso à cena ativa em runtime.

## Criar e instanciar

| Método | Descrição |
|--------|-----------|
| `CreateEntity(string name = "")` | Cria entidade vazia (Transform + Tag) |
| `InstantiatePrefab(string path)` | Instancia `.kzprefab` |
| `InstantiatePrefab(string path, Vector3 position)` | Instancia em uma posição |
| `InstantiatePrefab(string path, Vector3 position, Vector3 rotation)` | Instancia com posição **e rotação** (euler, rad) |
| `Duplicate(Entity)` | Duplica a entidade (com a subárvore), com leve deslocamento |

```csharp
var tiro = Scene.InstantiatePrefab("Assets/Tiro.kzprefab", new Vector3(3f, 0f, 0f));
var fogo = Scene.InstantiatePrefab("Assets/Fogo.kzprefab", pos, new Vector3(0f, 0f, 0.5f));
```

::: info
Em runtime, a prefab instanciada **ganha corpos de física** e **dispara
`OnCreate`** dos scripts — exatamente como se estivesse na cena.
:::

## Buscar e navegar

| Método | Descrição |
|--------|-----------|
| `Find(string name)` | Primeira entidade com o nome (Tag). `Entity.Invalid` se não achar |
| `GetPrimaryCamera()` | Entidade com `CameraComponent` marcada como Primary |
| `Load(string scenePath)` | **Pedido** de troca de cena (realizado no fim do frame) |

```csharp
var jogador = Scene.Find("Jogador");
if (jogador.IsValid) { /* achou */ }

var cam = Scene.GetPrimaryCamera();
Scene.Load("Assets/Fase2.kzscene"); // troca no fim do frame
```

## Queries de física

### 2D (Box2D)

```csharp
bool Raycast2D(Vector2 from, Vector2 to, out Entity hit, out Vector2 point);
```

### 3D (Bullet3)

```csharp
bool Raycast3D(Vector3 from, Vector3 to,
               out Entity hit, out Vector3 point, out float fraction);
```

### Área

```csharp
bool OverlapCircle2D(Vector2 center, float radius, out Entity hit);
bool OverlapSphere3D(Vector3 center, float radius, out Entity hit);
```

::: warn
**Só no Play.** As queries e a simulação da física só funcionam durante o
Play (ou no jogo exportado).
:::

## Exemplo de uso

```csharp
// cria um projétil na frente do jogador
if (Entity.TryGetWorldPosition(out var p))
{
    var tiro = Scene.CreateEntity("Tiro");
    tiro.AddSprite("Assets/Textures/tiro.png");
    tiro.SetPosition(p);

    // tiro em linha reta até acertar algo
    if (Scene.Raycast3D(p, p + new Vector3(0f, 0f, -50f), out var hit, out _, out _))
        Log.Info($"Tiro acertou {hit.Name}");
}
```
