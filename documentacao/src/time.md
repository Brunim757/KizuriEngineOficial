---
title: Time (API)
group: Scripting C#
order: 5
---

# Time — API

Controle de **tempo** do jogo.

## Propriedades

```csharp
Time.deltaTime;            // tempo do último frame (segundos)
Time.time;                 // tempo total de jogo
Time.unscaledTime;         // tempo real (ignora TimeScale)
Time.unscaledDeltaTime;    // delta real (ignora TimeScale)
```

## Escala de tempo

```csharp
// câmera lenta
Time.TimeScale = 0.3f;
// pausa (UI de pausa)
Time.TimeScale = 0f;
// volta ao normal
Time.TimeScale = 1f;
```

## Uso típico

```csharp
// movimento independente de FPS
Entity.Move(velocidade * Time.deltaTime, 0f, 0f);

// contagem regressiva
_tempoRestante -= Time.deltaTime;
if (_tempoRestante <= 0f) TerminarFase();
```

::: dica
Sempre multiplique velocidades por **Time.deltaTime** — senão o jogo anda
mais rápido em máquinas com mais FPS.
:::
