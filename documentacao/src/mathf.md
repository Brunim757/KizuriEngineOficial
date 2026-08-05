---
title: Mathf
group: Scripting C#
order: 7
---

# Mathf

Helpers matemáticos comuns de gameplay, estilo Unity. **Puramente managed.**

## Interpolação e suavização

| Método | Descrição |
|--------|-----------|
| `Clamp(v, min, max)` | Limita entre min e max (float e int) |
| `Lerp(a, b, t)` | Interpola com `t` clampeado em [0,1] |
| `LerpUnclamped(a, b, t)` | Interpola sem clamar `t` |
| `MoveTowards(cur, target, maxDelta)` | Move em direção ao alvo no máximo `maxDelta` por chamada |
| `SmoothDamp(cur, target, ref vel, smoothTime, dt)` | Seguimento suave (independe do framerate) |
| `SmoothStep(a, b, t)` | S-curve sem overshoot |

```csharp
pos = Mathf.Lerp(pos, alvo, 0.05f);            // aproximação suave
pos = Mathf.MoveTowards(pos, alvo, 10f * dt);  // velocidade constante
Mathf.SmoothDamp(pos, alvo, ref velocidade, 0.3f, dt);
```

## Ciclos e ondas

| Método | Descrição |
|--------|-----------|
| `Repeat(t, length)` | Valor em [0, length] que repete |
| `PingPong(t, length)` | Oscila 0→length→0 no ritmo de `t` |
| `Sin(v)` / `Cos(v)` | Seno/cosseno |

```csharp
float f = Mathf.PingPong(Time.time, 2f);  // 0 → 2 → 0 → 2 → …
float wave = Mathf.Sin(Time.time * 3f);   // oscilação suave
```

## Ângulos

| Método | Descrição |
|--------|-----------|
| `LerpAngle(a, b, t)` | Interpola ângulos pegando o **caminho curto** (350°→10°) |
| `Angle(Vector2 a, Vector2 b)` | Ângulo (graus) entre dois vetores 2D |

## Re-mapeamento

| Método | Descrição |
|--------|-----------|
| `Remap(value, fromMin, fromMax, toMin, toMax)` | Mapeia de um intervalo para outro |

```csharp
// 0..1 → -90..90
float angulo = Mathf.Remap(progresso, 0f, 1f, -90f, 90f);
```

## Aritmética e distância

| Método | Descrição |
|--------|-----------|
| `Abs(v)` / `Sign(v)` / `Min(a,b)` / `Max(a,b)` | Básicos |
| `Sqrt(v)` / `Pow(v, e)` | Potência |
| `Distance(Vector2 a, Vector2 b)` | Distância 2D |
| `Distance(Vector3 a, Vector3 b)` | Distância 3D |

```csharp
if (Mathf.Distance(jogadorPos, alvoPos) < 2f) { }
```

## Referência rápida (assinaturas)

```csharp
float Clamp(float v, float min, float max);
int   Clamp(int v, int min, int max);
float Lerp(float a, float b, float t);
float LerpUnclamped(float a, float b, float t);
float MoveTowards(float current, float target, float maxDelta);
float SmoothDamp(float current, float target, ref float velocity, float smoothTime, float dt);
float SmoothStep(float a, float b, float t);
float Repeat(float t, float length);
float PingPong(float t, float length);
float Remap(float value, float fromMin, float fromMax, float toMin, float toMax);
float LerpAngle(float a, float b, float t);
float Angle(Vector2 a, Vector2 b);
float Distance(Vector2 a, Vector2 b);
float Distance(Vector3 a, Vector3 b);
```
