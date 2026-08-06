---
title: Física 3D
group: Componentes
order: 8
---

# Física 3D

A física 3D usa **Bullet**: corpos rígidos, colisores de caixa/esfera e
forças — ideal para jogos 3D, veículos e quebra-cabeças.

## Componentes

| Componente | O que faz |
|---|---|
| **Rigidbody3D** | Corpo físico (estático/dinâmico/cinemático) |
| **BoxCollider3D** | Caixa de colisão |
| **SphereCollider3D** | Esfera de colisão |

## Rigidbody3D

- **Type**: estático, dinâmico ou cinemático
- Forças via script: `ApplyForce`, `ApplyImpulse`
- Velocidade: `TryGetVelocity`, `SetVelocity`
- Torque: `ApplyTorque`, `SetAngularVelocity`

## Consultas

- **Scene.Raycast3D** — o raio mais próximo contra os corpos (tiro, mira)
- **Scene.OverlapSphere3D** — quais corpos estão numa região (explosão,
  área de dano)

## Scripting

```csharp
// lançar um corpo
Entity.ApplyImpulse(0f, 5f, -3f);
// checar o que está numa área
foreach (var e in Scene.OverlapSphere3D(posX, posY, posZ, 2f))
    Scene.Destroy(e);
```

::: dica
O Rigidbody3D é registrado sob demanda: corpos criados em **runtime**
entram na física no primeiro frame do update — pode instanciar à vontade.
:::

Veja também [Física 2D](fisica-2d.html).
