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
Entity.SetLinearVelocity(x, y);
// detectar toque
public override void OnCollisionBegin(Entity other) {
    if (other.Name == "Moeda") Scene.Destroy(other);
}
```

## OverlapCircle2D

O **Raycast2D** e o **OverlapCircle2D** ajudam em plataformas:
`Scene.Raycast2D(origem, direção)` e `Scene.OverlapCircle2D(posição, raio)`.

::: dica
Para física **3D**, veja [Física 3D](fisica-3d.html). Para colisões de
tilemap, veja [Tilemap](tilemap.html).
:::
