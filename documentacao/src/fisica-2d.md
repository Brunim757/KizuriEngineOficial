---
title: Física 2D (Box2D)
group: Componentes
order: 11
---

# Física 2D (Box2D)

Corpos rígidos e colliders em 2D, simulados pelo **Box2D** durante o Play.

## Rigidbody2DComponent

| Campo | Descrição |
|-------|-----------|
| **Type** | `Static` (não move), `Dynamic` (responde a forças), `Kinematic` (move via script) |
| **FixedRotation** | Trava a rotação do corpo |
| **GravityScale** | Multiplicador da gravidade: **<0 inverte**, **0 desliga** |

### GravityScale — a chave do platformer

```csharp
Entity.SetGravityScale(0f);    // sem gravidade (personagem aéreo)
Entity.SetGravityScale(-1f);   // gravidade INVERTIDA (teto!);
Entity.SetGravityScale(0.5f);  // gravidade mais leve (lua)
```

## Colliders 2D

| Componente | Campos |
|------------|--------|
| **BoxCollider2D** | Offset, Size, Density, Friction, Restitution, RestitutionThreshold |
| **CircleCollider2D** | Offset, Radius, Density, Friction, Restitution |

## Controlando o corpo

```csharp
if (Entity.TryGetRigidbody2D(out var rb))
{
    var v = rb.GetLinearVelocity();
    rb.SetLinearVelocity(new Vector2(wish.X, v.Y));  // controle lateral
    rb.ApplyLinearImpulse(new Vector2(0f, 8f));      // pulo
    rb.SetTransform(new Vector2(x, y), angleRad);    // teleportar
}
```

## Queries

```csharp
// raio de A até B — devolve entidade e ponto do impacto
if (Scene.Raycast2D(from, to, out var hit, out var point))
    Log.Info($"Atingiu {hit.Id} em {point}");

// algum collider toca o círculo?
if (Scene.OverlapCircle2D(center, radius, out var closest))
    Log.Info($"Círculo tocou {closest.Id}");
```

## Tilemap sólido

No [Tilemap](tilemap.html), os valores em `SolidTileValues` geram colliders
estáticos automaticamente — níveis de platformer sem montar colisor a colisor.

## Callbacks de colisão

No script:

```csharp
public override void OnCollisionBegin(Entity other) { }
public override void OnCollisionEnd(Entity other) { }
```

::: warn
A física 2D **só simula no Play** — no modo de edição os corpos ficam
parados no Transform.
:::

## Dicas

- **Dynamic + Kinematic**: use Kinematic para plataformas móveis; o
  `SetTransform` sincroniza o corpo com o Transform (teleporte limpo);
- **Restitution > 0** para objetos que quicam (bolas);
- **Friction baixa** (0.1) para objetos que deslizam.
