---
title: Física 3D
group: Componentes
order: 8
---

# Física 3D

A física 3D usa **Bullet**: corpos rígidos, colisores de caixa/esfera,
**terreno (heightmap)** e **character controller com colisão de paredes** —
ideal para jogos 3D, veículos e mundos abertos.

## Componentes

| Componente | O que faz |
|---|---|
| **Rigidbody3D** | Corpo físico (estático/dinâmico/cinemático) |
| **BoxCollider3D** | Caixa de colisão |
| **SphereCollider3D** | Esfera de colisão |
| **Terreno + Rigidbody3D (estático)** | **Heightfield** do Bullet — objetos e personagens andam no relevo |
| **CharacterController** | Cápsula com gravidade, colisão com **paredes**, **step** e rampas |

## Terreno com colisão (v0.31)

1. Adicione **Terreno** (Inspetor → + Adicionar Componente) — o editor já
   cria o **Rigidbody3D estático** automaticamente.
2. Aperte Play: o relevo ganha um **heightfield** e as entidades/objetos
   físicos caem e andam sobre ele.

```csharp
// Terreno em runtime via script:
Entity.AddTerrain(64, 60f, 6f, 42); // segmentos, tamanho, elevação, semente
```

## Character Controller (v2 — colide com paredes)

A cápsula física **esbarra em paredes, sobe degraus (step), escorrega em
rampas e cai com gravidade** — bem melhor que a v1 (só raycast de chão).

```csharp
Entity.AddCharacterController(6f, -20f); // velocidade, gravidade
Entity.MoveCharacter(dirX, dirZ);        // input de movimento todo frame
```

## Rigidbody3D

- **Type**: estático, dinâmico ou cinemático
- **GravityScale** — gravidade por corpo (`0` = flutua, negativo = invertida)
- **Damping** (linear/angular) — movimento mais macio
- Forças via script: `ApplyForce`, `ApplyImpulse`
- Velocidade: `TryGetVelocity`, `SetVelocity`
- Torque: `ApplyTorque`, `SetAngularVelocity`

```csharp
Entity.SetGravityScale3D(0f);           // flutua (gravidade zerada)
Entity.SetDamping3D(0.1f, 0.05f);       // amortecimento linear/angular
```

## Consultas

- **Scene.Raycast3D** — o raio mais próximo contra os corpos (tiro, mira)
- **Scene.OverlapSphere3D** — primeiro corpo numa região (explosão)
- **Scene.OverlapBox3D** — primeiro corpo numa **caixa** (área de plataforma)
- **Scene.OverlapSphereAll3D** — **TODOS** os corpos na área (sensor de dano,
  zona de captura)

## Scripting

```csharp
// lançar um corpo
Entity.ApplyImpulse(0f, 5f, -3f);
// checar o que está numa área (todos)
foreach (var e in Scene.OverlapSphereAll3D(pos, 2f))
    Scene.Destroy(e);
// área retangular
if (Scene.OverlapBox3D(centro, metades, out var alvo))
    Alvo.Active = false;
```

::: dica
O Rigidbody3D é registrado sob demanda: corpos criados em **runtime**
entram na física no primeiro frame do update — pode instanciar à vontade.
:::

::: aviso
A física só simula durante o **Play** (mesma convenção da 2D). No editor o
viewport mostra a cena sem simulação.
:::

Veja também [Física 2D](fisica-2d.html).

