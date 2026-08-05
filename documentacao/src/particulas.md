---
title: Partículas
group: Componentes
order: 7
---

# Partículas

Emissor de partículas **GPU-instanced** com billboarding. Fogo, faíscas,
magia, fumaça — tudo com o mesmo componente.

## ParticleSystemComponent

| Campo | Descrição |
|-------|-----------|
| **Playing** | Emissor ligado? |
| **Additive** | `true` = aditivo (fogo/faísca/magia); `false` = alpha normal (fumaça) |
| **EmissionRate** | Partículas por segundo |
| **MaxParticles** | Teto de partículas vivas |
| **LifetimeMin / LifetimeMax** | Vida das partículas (aleatória no intervalo) |
| **VelocityMin / VelocityMax** | Velocidade inicial (aleatória no intervalo) |
| **Gravity** | Aceleração aplicada às partículas |
| **StartColor / EndColor** | Cor interpolada ao longo da vida |
| **StartSize / EndSize** | Tamanho interpolado |
| **TexturePath** | Textura opcional (vazia = **degradê radial procedural**) |

## Exemplos de receitas

| Efeito | Configuração |
|--------|--------------|
| **Fogo** | Additive on, cor 1.0,0.65,0.15 → 1.0,0.15,0.02, gravidade −2 |
| **Fumaça** | Additive off, cinza claro, tamanho crescendo |
| **Faíscas** | Additive on, amarelo, gravidade −9 |
| **Magia** | Additive on, StartColor azul/ciano, EndColor transparente |

## Em C#

```csharp
var fogo = Scene.CreateEntity("Fogo");
fogo.AddMeshRenderer("builtin:cube"); // ainda não há AddParticles no C#
fogo.SetParticleTexture("Assets/Textures/faisca.png"); // textura opcional
```

::: warn
**Simula no Play.** Partículas (como física e áudio) só emitem durante o
**Play** — no modo de edição o emissor fica parado.
:::

## No editor

- Ajuste os campos no Inspetor e veja o efeito em tempo real durante o Play;
- **Modo aditivo** é uma checkbox (fogo/faíscas = aditivo; fumaça = normal);
- Sem textura, o degradê radial procedural já dá um resultado bom para
  partículas suaves.
