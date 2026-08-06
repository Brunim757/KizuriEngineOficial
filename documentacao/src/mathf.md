---
title: Mathf, Rand e Vetores
group: Scripting C#
order: 6
---

# Mathf, Rand e Vetores

Utilitários matemáticos para jogos.

## Mathf

```csharp
Mathf.Clamp(valor, min, max);
Mathf.Lerp(a, b, t);            // interpolação linear
Mathf.LerpAngle(a, b, t);       // interpolação de ângulos (curto caminho)
Mathf.SmoothStep(a, b, t);      // interpolação suave
Mathf.PingPong(t, comprimento); // vai e volta entre 0 e comprimento
Mathf.Remap(valor, a1, b1, a2, b2); // re-mapeia de uma faixa para outra
Mathf.Angle(de, para);          // menor diferença angular
```

## Rand

```csharp
Rand.Float(0f, 1f);     // float aleatório
Rand.Int(1, 6);         // int aleatório (1..5)
Rand.Range(-1f, 1f);
```

## Vetores

As entidades usam `Vector3`:

```csharp
var v = Entity.Position;
v.X += 1f;
Entity.Position = v;

var dir = new Vector3(0f, 1f, 0f);
Vector3.Distance(a, b);
Vector3.Dot(a, b);
Vector3.Cross(a, b);
Vector3.Normalize(a);
Vector3.Lerp(a, b, t);
```

::: dica
`Mathf.Remap` é ouro para barras de vida: remapeie a vida (0..100) para a
largura da barra (0..1).
:::
