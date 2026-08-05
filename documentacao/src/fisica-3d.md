---
title: Física 3D (Bullet3)
group: Componentes
order: 12
---

# Física 3D (Bullet3)

Corpos rígidos e colliders em 3D, simulados pelo **Bullet3** durante o Play.

## Rigidbody3DComponent

| Campo | Descrição |
|-------|-----------|
| **Type** | `Static` / `Dynamic` / `Kinematic` |
| **Mass** | Massa do corpo (para Dynamic) |

## Colliders 3D

| Componente | Campos |
|------------|--------|
| **BoxCollider3D** | `HalfExtents` (meias-dimensões: 0.5,0.5,0.5 = cubo de 1×1×1) |
| **SphereCollider3D** | `Radius` |

::: warn
**HalfExtents ≠ tamanho.** O collider de caixa usa **meias-dimensões**. Para
uma caixa de 2×2×2 use `HalfExtents = (1, 1, 1)`.
:::

## Forças e movimento

```csharp
Entity.AddRigidbody3D(BodyType3D.Dynamic, mass: 1f);
Entity.AddBoxCollider3D(0.5f, 0.5f, 0.5f);
// ou
Entity.AddSphereCollider3D(radius: 0.5f);

Entity.ApplyForce(new Vector3(0f, 10f, 0f));       // força contínua
Entity.ApplyImpulse(new Vector3(0f, 5f, 0f));      // impulso instantâneo
Entity.ApplyTorque(new Vector3(0f, 3f, 0f));       // giro

Entity.SetVelocity(new Vector3(1f, 0f, 0f));       // velocidade linear
Entity.SetAngularVelocity(new Vector3(0f, 1f, 0f));// velocidade angular

if (Entity.TryGetVelocity(out var v)) { }
if (Entity.TryGetAngularVelocity(out var w)) { }
```

## Queries 3D

```csharp
// raio no espaço 3D — devolve entidade, ponto e fração [0,1]
if (Scene.Raycast3D(from, to, out var hit, out var point, out var fraction))
    Log.Info($"Atingiu {hit.Id} em {point} (fracão {fraction:0.00})");

// esfera de área
if (Scene.OverlapSphere3D(center, radius, out var hit))
    Log.Info($"Esfera tocou {hit.Id}");
```

## Raycast vs. Overlap

| Query | Uso típico |
|-------|-----------|
| `Raycast3D` | Tiro, mira, "olhando para", detecção de chão |
| `OverlapSphere3D` | Explosões, área de dano, sensor de proximidade |

::: warn
A física 3D **só simula no Play**.
:::

## Dicas

- **Mass pequena** = corpo leve que voa com facilidade; **mass alta** = pesado;
- **Kinematic** não responde a forças — mova pelo `SetTransform`/posição;
- Combine com [MeshRenderer](meshes-3d.html): a posição do corpo 3D segue o
  Transform da entidade.
