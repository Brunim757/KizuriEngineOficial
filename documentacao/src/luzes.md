---
title: Luzes
group: Componentes
order: 6
---

# Luzes

Entidades com **LightComponent** são luzes reais na cena. Há três tipos.

## Tipos

| Tipo | Descrição |
|------|-----------|
| **Direcional** | Luz paralela sem posição (o "sol"). A **direção** vem da rotação da entidade. A **primeira** direcional da cena projeta sombra em cascata. |
| **Ponto** | Luz radial com alcance (`Range`). Projeta sombra quando o hardware suporta. |
| **Spot** | Cone com ângulos interno/externo em graus. Projeta sombra se `CastsShadow`. |

## Propriedades

| Campo | Descrição |
|-------|-----------|
| **Type** | Direcional / Ponto / Spot |
| **Color** | Cor RGB |
| **Intensity** | Intensidade (multiplicador HDR) |
| **Range** | Alcance (Ponto/Spot) |
| **InnerConeDeg / OuterConeDeg** | Ângulos do cone do spot, em graus |
| **CastsShadow** | Ponto/Spot projetam sombra? (automático pelo hardware) |

## Limites

- Até **16 luzes** por frame (todas avaliadas no forward pass);
- O custo das luzes é por pixel — luzes de ponto com `Range` grande custam
  mais.

## Em C#

```csharp
var sol = Scene.CreateEntity("Sol");
sol.AddLight(LightType.Directional, r: 1f, g: 0.95f, b: 0.85f, intensity: 2f);
sol.SetRotation(new Vector3(Mathf.Deg2Rad…, 0f, 0f)); // aponta a luz

var lanterna = Scene.CreateEntity("Lanterna");
lanterna.AddLight(LightType.Spot, intensity: 5f,
                  innerConeDeg: 20f, outerConeDeg: 30f, castsShadow: true);

lanterna.SetLightColor(1f, 0.8f, 0.5f);
lanterna.SetLightIntensity(8f);
```

## No editor

Selecione a luz e use o **gizmo de luz** no viewport para posicionar e apontar.
O gizmo mostra a direção (direcional) ou o cone/alcance (ponto/spot).

::: info
Sombras de luz pontual dependem do hardware — o motor liga automaticamente
quando a placa suporta.
:::
