---
title: Iluminação
group: Gráficos
order: 3
---

# Iluminação

A iluminação da Kizuri é **física** (PBR) com luzes diretas, reflexos do
ambiente (IBL) e iluminação indireta aproximada.

## Luzes

Cada entidade pode ter uma luz:

| Tipo | Uso |
|---|---|
| **Direcional** | O sol: ilumina tudo de uma direção, projeta sombras (CSM) |
| **Ponto** | Lâmpada: ilumina a partir de um ponto, com alcance |
| **Spot** | Holofote: cone com ângulo interno/externo |

No Inspetor: cor, intensidade, alcance (ponto/spot) e cone (spot).

## SSAO — oclusão de ambiente

Escurece cantos e frestas onde a luz não chega, dando "peso" aos objetos.
Ajustes: **amostras** e **raio**.

## SSGI — iluminação global

A **luz indireta**: o SSR marcha raios no hemisfério e coleta a **cor** dos
objetos vizinhos, devolvendo aquela "luz que quica" entre superfícies —
paredes coloridas tingem o ambiente, sombras ganham preenchimento.

- Ajuste: **intensidade** (exagere devagar; valores ~0,4 já fazem efeito)

## IBL — iluminação baseada no ambiente

O céu (físico ou HDRI) vira uma fonte de luz para os materiais PBR:
**irradiância** para o difuso e **reflexos pré-filtrados** para o especular.
Ou seja: a cor do céu ilumina e reflete nos objetos automaticamente.

::: dica
Para a "hora mágica", baixe o sol (rotacione a luz direcional) — o céu
físico muda para laranja/vermelho e a iluminação acompanha.
:::
