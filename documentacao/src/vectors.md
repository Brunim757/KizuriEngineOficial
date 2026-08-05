---
title: Vetores (Vector2/3/4)
group: Scripting C#
order: 8
---

# Vetores

Tipos matemáticos públicos do jogo em `Kizuri.Math`. Nada de glm: a engine
expõe números, e estas structs são o "Vector3" do Kizuri C#.

## Vector2

```csharp
public struct Vector2
{
    public float X, Y;
    public static Vector2 Zero;
    public float Length;          // magnitude
}
```

Operadores: `+`, `-`, `*` (escalar), `/` (escalar).

## Vector3

```csharp
public struct Vector3
{
    public float X, Y, Z;
    public static Vector3 Zero;
    public static Vector3 One;
    public float Length;
}
```

Construtores:

```csharp
new Vector3(1f, 2f, 3f);        // x, y, z
new Vector3(2f);                // todos = 2
new Vector3(new Vector2(1f,2f), 3f); // x=1, y=2, z=3
```

Operadores: `+`, `-`, `*` (escalar).

## Vector4

```csharp
public struct Vector4 { public float X, Y, Z, W; }
```

Operadores: `*` (escalar).

## Uso típico

```csharp
using Kizuri.Math;

var pos = new Vector3(1f, 0f, 0f);
pos.X += 2f;                          // campo mutável
Entity.SetPosition(pos);

var dir = (alvo - origem);
var dist = dir.Length;

// normalizar manualmente (não há .normalized ainda)
if (dist > 0.0001f) dir = dir * (1f / dist);
```

::: info
**Structs mutáveis.** Os campos X/Y/Z são públicos e mutáveis — você pode
fazer `t.Translation.X += dt` direto sobre um `Transform` obtido via
`TryGetTransform` e depois `SetPosition`/`SetRotation` para aplicar.
:::

## Dica: movimento por frame

```csharp
if (Entity.TryGetTransform(out var t))
{
    t.Translation.X += wish.X * deltaSeconds;
    t.Translation.Y += wish.Y * deltaSeconds;
    Entity.SetPosition(t.Translation);
}
```
