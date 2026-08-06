---
title: Luzes
group: Componentes
order: 5
---

# Luzes

O componente **Light** adiciona uma fonte de luz PBR à cena.

## Tipos

| Tipo | Descrição |
|---|---|
| **Direcional** | Luz sem posição, vem de uma direção (o sol). Única que projeta sombra (CSM) |
| **Ponto** | Lâmpada radial com alcance e atenuação |
| **Spot** | Cone de luz com ângulo interno (cheio) e externo (borda suave) |

## Propriedades

- **Cor** e **Intensidade**
- **Range** (ponto/spot) — até onde a luz chega
- **Inner/Outer cone** (spot) — graus do cone interno e externo
- **CastsShadow** — (a direcional projeta sombra sempre; ponto/spot iluminam sem sombra)

## Dicas

- **1 luz direcional = sol**: controle o pôr-do-sol girando a direção (o céu
  físico acompanha).
- Até **16 luzes** por cena no passe forward.
- Luzes pontuais e spots **não projetam sombra** — para um "holofote com
  sombra", use texturas de silhueta ou aproxime com a direcional.

::: dica
A cor do ambiente (IBL) vem do céu — veja [Céu](ceu.html) e
[Iluminação](iluminacao.html).
:::
