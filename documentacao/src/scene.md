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

## Consultas (raycasts)

```csharp
// 2D (Box2D)
var hit = Scene.Raycast2D(origemX, origemY, dirX, dirY);
var alvos = Scene.OverlapCircle2D(x, y, raio);

// 3D (Bullet)
var hit3D = Scene.Raycast3D(origX, origY, origZ, dirX, dirY, dirZ);
var corpos = Scene.OverlapSphere3D(x, y, z, raio);
```

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
