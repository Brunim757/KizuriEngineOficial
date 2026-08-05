---
title: Transform
group: Componentes
order: 1
---

# Transform

Toda entidade tem um **Transform** — a posição, rotação e escala no mundo.

## Propriedades

| Campo | Tipo | Descrição |
|-------|------|-----------|
| **Translation** | `Vector3` | Posição local em unidades de mundo |
| **Rotation** | `Vector3` (euler) | Rotação em **radianos** |
| **Scale** | `Vector3` | Escala local (1,1,1 = 100%) |

No Inspetor a rotação é exibida em **graus**; no C#, sempre em **radianos**.

## Hierarquia

O transform é **local** quando a entidade tem pai. Movendo o pai, os filhos
acompanham. A posição *mundial* soma a cadeia inteira de pais.

## Em C#

```csharp
Entity.SetPosition(new Vector3(1f, 0f, 0f));
Entity.SetRotation(new Vector3(0f, 0f, 0.5f));  // radianos
Entity.SetScale(new Vector3(2f, 2f, 2f));

if (Entity.TryGetTransform(out var t)) { }
if (Entity.TryGetWorldPosition(out var world)) { } // posição mundial
Entity.LookAt(target);                              // encara um ponto
```

## No editor

Use os gizmos (<kbd>W</kbd>/<kbd>E</kbd>/<kbd>R</kbd>) para editar o transform
visualmente, ou digite valores no Inspetor.

::: warn
**Ordem TRS.** A matriz é `T · R · S`. Escalar um pai com filhos estica os
filhos proporcionalmente.
:::

## Rotação euler

A rotação usa **euler** (pitch, yaw, roll em X, Y, Z). Em ângulos como -90°,
é comum combinar dois eixos para efeitos de câmera — por exemplo, uma câmera
olhando para frente no eixo -Z:

```csharp
cam.SetRotation(new Vector3(
    -0.21f,   // pitch (rad) — olha um pouco para baixo
    -1.571f,  // yaw (rad)   — -90°
    0f));
```

Dica: use `Mathf.Deg2Rad`-style `float rad = Mathf.Sin/…` ou a constante
`MathF.PI / 180f * graus`.
