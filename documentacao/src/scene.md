---
title: Scene (API)
group: Scripting C#
order: 3
---

# Scene — API

A **Scene** é o mundo do jogo. O script acessa a cena atual via `Scene`.

## Criar e destruir entidades

```csharp
var entidade = Scene.CreateEntity("Inimigo");
entidade.AddSprite("Assets/imgs/zumbi.png");
Scene.Destroy(entidade);
```

## Prefabs

```csharp
Scene.InstantiatePrefab("Assets/Prefabs/tiro.kzprefab", x, y, z);
Scene.InstantiatePrefab(path, x, y, z, rotX, rotY, rotZ);
```

## Trocar de cena

```csharp
Scene.Load("Assets/Cenas/fase2.kzscene"); // carregamento diferido/assíncrono
```

## Câmera

```csharp
var cam = Scene.GetPrimaryCamera();
```

## Buscar entidades

```csharp
// Primeira entidade com o nome dado
var inimigo = Scene.Find("Inimigo");

// Todas as entidades que têm o Tag "Inimigo" (Tags & Layers)
Entity[] horda = Scene.EntitiesWithTag("Inimigo");
foreach (var e in horda) { ... }
```

## Consultas (raycasts)

```csharp
// 2D (Box2D)
var hit = Scene.Raycast2D(origemX, origemY, dirX, dirY);
var alvos = Scene.OverlapCircle2D(x, y, raio);

// 3D (Bullet)
var hit3D = Scene.Raycast3D(origX, origY, origZ, dirX, dirY, dirZ);
var corpos = Scene.OverlapSphere3D(x, y, z, raio);
```

## Instancing de malhas (floresta/multidão)

Desenhe a MESMA malha em N posições num **único draw call**:

```csharp
// monta N transformadas (posição + escala)
var transforms = new float[count * 16];
for (int i = 0; i < count; i++)
    Array.Copy(Scene.MakeTransform(x[i], 0f, z[i], 1f), 0, transforms, i * 16, 16);

Scene.DrawInstanced("builtin:cube", new Math.Vector3(0.3f, 0.6f, 0.2f), transforms, count);
```

Chame dentro de `OnUpdate` — cada frame desenha as N cópias em um draw call.

## Corrotinas

```csharp
StartCoroutine(Contador());

IEnumerator Contador()
{
    yield return new WaitForSeconds(2f);
    Scene.Destroy(Entity);
}
```

::: dica
O **Load** de cena é assíncrono — o jogo não congela, e a conclusão religa o
runtime automaticamente. Veja [Save](save.html) para persistência.
:::
