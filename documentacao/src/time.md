---
title: Time
group: Scripting C#
order: 6
---

# Time

Tempo de jogo e do mundo.

## Propriedades

| Membro | Tipo | Descrição |
|--------|------|-----------|
| `Time.DeltaSeconds` | `float` | Delta deste frame (escalado pelo `TimeScale`) |
| `Time.time` | `float` | Tempo acumulado desde o início (escalado) |
| `Time.unscaledTime` | `float` | Tempo real acumulado (ignora `TimeScale`) |
| `Time.unscaledDeltaTime` | `float` | Delta real do frame |
| `Time.TimeScale` | `float` (get/set) | Escala global do tempo |

## Uso

```csharp
var dt = Time.DeltaSeconds;                 // para o movimento
var t  = Time.time;                         // para animações que dependem do tempo

// câmera lenta
Time.TimeScale = 0.3f;
// pausa os SCRIPTS (dt vira 0)
Time.TimeScale = 0f;
// volta ao normal
Time.TimeScale = 1f;
```

## O que o TimeScale afeta

- **Escala** o `deltaSeconds` recebido pelos scripts (o `dt` do `OnUpdate`);
- Não trava a física/partículas (que usam dt próprio) — `TimeScale = 0`
  pausa os **scripts**, não o mundo inteiro.

```csharp
// movimento independente da câmera lenta (UI, contadores)
var real = Time.unscaledDeltaTime;
```

## Timestep

O struct `Timestep` envolve o delta e **converte implicitamente para float**:

```csharp
Timestep ts = new(0.016f);
float seconds = ts.Seconds;   // ou simplesmente (float)ts
```

::: info
No `OnUpdate(float deltaSeconds)` você já recebe o delta pronto — não precisa
chamar `Time.DeltaSeconds` de novo para o movimento do frame.
:::
