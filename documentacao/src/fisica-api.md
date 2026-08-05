---
title: Rigidbody2D e queries
group: Scripting C#
order: 12
---

# Física na API C#

A struct `Rigidbody2D` é devolvida por `Entity.TryGetRigidbody2D` — controle
direto do corpo 2D.

## Rigidbody2D

| Membro | Descrição |
|--------|-----------|
| `Type` | `BodyType` (Static/Dynamic/Kinematic) |
| `GetLinearVelocity()` | Velocidade atual (`Vector2`) |
| `SetLinearVelocity(Vector2)` | Define a velocidade |
| `ApplyLinearImpulse(Vector2, bool wake = true)` | Impulso instantâneo |
| `SetTransform(Vector2 pos, float angleRad)` | Sincroniza/teleporta o corpo |

```csharp
public override void OnUpdate(float deltaSeconds)
{
    var wish = Vector2.Zero;
    if (Input.IsKeyPressed(Key.A)) wish.X -= 1f;
    if (Input.IsKeyPressed(Key.D)) wish.X += 1f;

    if (Entity.TryGetRigidbody2D(out var rb))
    {
        var v = rb.GetLinearVelocity();
        rb.SetLinearVelocity(new Vector2(wish.X * 5f, v.Y));  // controle lateral
    }
}
```

### Pulo

```csharp
if (Input.IsKeyDown(Key.Space))
    if (Entity.TryGetRigidbody2D(out var rb))
        rb.ApplyLinearImpulse(new Vector2(0f, 8f));
```

## Queries de física

Todas devolvem `false`/`Entity.Invalid` quando nada foi atingido. **Só
funcionam no Play.**

| Query | Assinatura |
|-------|------------|
| `Scene.Raycast2D` | `(Vector2 from, Vector2 to, out Entity hit, out Vector2 point)` |
| `Scene.Raycast3D` | `(Vector3 from, Vector3 to, out Entity hit, out Vector3 point, out float fraction)` |
| `Scene.OverlapCircle2D` | `(Vector2 center, float radius, out Entity hit)` |
| `Scene.OverlapSphere3D` | `(Vector3 center, float radius, out Entity hit)` |

```csharp
// 2D — raio para baixo
if (Scene.Raycast2D(pos2d, pos2d + new Vector2(0f, -20f), out var chao, out var ponto))
    if (chao.Name == "Chão") estaNoChao = true;

// 3D — área de explosão
if (Scene.OverlapSphere3D(centro, 5f, out var alvo))
    alvo.Destroy();
```

## Corpos 3D

A física 3D (Bullet3) é controlada por métodos diretos da `Entity`:

- `AddRigidbody3D(BodyType3D, mass)` + `AddBoxCollider3D` / `AddSphereCollider3D`;
- `ApplyForce` / `ApplyImpulse` / `ApplyTorque`;
- `SetVelocity` / `SetAngularVelocity` / `TryGetVelocity` / `TryGetAngularVelocity`.

Ver [Física 3D](fisica-3d.html).

## Tipos de corpo

```csharp
enum BodyType   { Static = 0, Dynamic, Kinematic }   // 2D
enum BodyType3D { Static = 0, Dynamic, Kinematic }   // 3D
```

| Tipo | Responde a forças? | Move por script? |
|------|--------------------|------------------|
| **Static** | não | não (cena fixa) |
| **Dynamic** | sim | sim (mas a física manda) |
| **Kinematic** | não | sim (`SetTransform`/posição) |
