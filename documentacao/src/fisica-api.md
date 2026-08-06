---
title: Física (API C#)
group: Scripting C#
order: 8
---

# Física — API C#

Acesse física 2D e 3D direto do script.

## Rigidbody2D

```csharp
Entity.AddRigidbody2D();               // dinâmico por padrão
Entity.SetLinearVelocity(vx, vy);      // velocidade linear
Entity.GravityScale = 0.5f;            // <0 flutua, 0 sem gravidade
```

## Rigidbody3D

```csharp
Entity.AddRigidbody3D();
Entity.ApplyForce(fx, fy, fz);
Entity.ApplyImpulse(ix, iy, iz);
Entity.ApplyTorque(tx, ty, tz);
Entity.TryGetVelocity(out vx, out vy, out vz);
Entity.SetVelocity(vx, vy, vz);
```

## Colisões

```csharp
public override void OnCollisionBegin(Entity other)
{
    if (other.Name == "Chão") noChao = true;
}

public override void OnCollisionEnd(Entity other) { }
```

## Consultas

```csharp
// 2D
var hit = Scene.Raycast2D(ox, oy, dx, dy);
var circles = Scene.OverlapCircle2D(x, y, raio);

// 3D
var hit3D = Scene.Raycast3D(ox, oy, oz, dx, dy, dz);
var spheres = Scene.OverlapSphere3D(x, y, z, raio);
```

::: dica
Corpos criados em **runtime** entram na física no primeiro frame — pode
`InstantiatePrefab` um inimigo com rigidbody sem se preocupar com registro.
:::

Veja também [Entity](entity.html) e [Física 2D](fisica-2d.html) /
[Física 3D](fisica-3d.html).
