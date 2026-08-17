---
title: Física 2D
group: Componentes
order: 7
---

# Física 2D

A física 2D usa **Box2D**: gravidade, colisões e contatos confiáveis para
jogos de plataforma, pinball, quebra-cabeças e qualquer coisa 2D.

## Componentes

| Componente | O que faz |
|---|---|
| **Rigidbody2D** | Corpo físico: estático, dinâmico ou cinemático |
| **BoxCollider2D** | Colisor retangular |
| **CircleCollider2D** | Colisor circular |

## Rigidbody2D

- **Type**: `Estático` (não se move, ex.: chão), `Dinâmico` (aplica física),
  `Cinemático` (movido por código, empurra outros)
- **GravityScale** — gravidade por corpo (`0` = sem gravidade, negativo = flutua)
- **LinearVelocity** — velocidade linear (defina no script)

## Colisões

- Colisores **não precisam** de rigidbody (um "sensor" estático só detecta).
- Colisão entre dois dinâmicos resolve impulso automaticamente.
- Para eventos de colisão no script, use `OnCollisionBegin`/`OnCollisionEnd`.

## Scripting

```csharp
// mover um corpo dinâmico
var rb = Entity.GetRigidbody2D();
rb.SetLinearVelocity(new Vector2(5f, 0f));
rb.ApplyForce(new Vector2(0f, 100f));       // força contínua (todo frame)
rb.ApplyLinearImpulse(new Vector2(0f, 20f)); // impulso único (pulo)
rb.SetAngularVelocity(3f);                         // gira (rad/s)
rb.SetFixedRotation(true);                         // não tomba (tank)
// detectar toque
public override void OnCollisionBegin(Entity other) {
    if (other.Name == "Moeda") Scene.Destroy(other);
}
```

`Entity.GetRigidbody2D()` devolve um `Rigidbody2D` (ou use `TryGetRigidbody2D`
com `out`). Tudo aplicado em runtime, durante o Play.

## Tilemap com colisão em runtime

Além do tilemap do editor, dá pra construir um nível no script — tiles
marcados como sólidos geram **collider Box2D estático** automaticamente:

```csharp
var mapa = Scene.CreateEntity("Mapa");
mapa.AddTilemap("Assets/imgs/atlas.png", atlasCols: 8, atlasRows: 8, mapW: 20, mapH: 12);
mapa.AddSolidTile(3);                 // tile 3 do atlas = sólido
mapa.SetTile(2, 3, 3);                // coloca o tile sólido
// y=0 é a linha de cima; colliders recriam sozinhos quando o mapa muda
```

## OverlapCircle2D

O **Raycast2D** e o **OverlapCircle2D** ajudam em plataformas:
`Scene.Raycast2D(origem, direção)` e `Scene.OverlapCircle2D(posição, raio)`.

::: dica
Para física **3D**, veja [Física 3D](fisica-3d.html). Para colisões de
tilemap, veja [Tilemap](tilemap.html).
:::
