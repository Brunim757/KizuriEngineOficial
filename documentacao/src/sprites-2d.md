---
title: Sprites 2D
group: Componentes
order: 4
---

# Sprites 2D

O componente **SpriteRenderer** desenha uma imagem 2D na cena.

## Como usar

1. Traga uma imagem para o **Content Browser**.
2. **Arraste a imagem para o viewport** (cria a entidade com sprite) ou
   arraste para o campo **Sprite** do componente.
3. No Inspetor, ajuste a **cor** (tinta), a **ordem de desenho** e o **flip**.

## Propriedades

- **Sprite** — a textura
- **Cor** — tint (branco = original, preto = só silhueta)
- **FlipX / FlipY** — espelha no eixo (para personagens virando de lado)
- **SortingLayer** — camada de desenho (maior desenha por cima)
- **Tile** — repete a textura dentro do retângulo

## Dicas

- Para **animação de sprite** (folhas com quadros), veja
  [Animação 2D](animacao-2d.html).
- Para **tilemaps**, veja [Tilemap](tilemap.html).
- Sprites usam a câmera **ortográfica principal** — veja [Câmera](camera.html).

::: nota
A ordem de desenho no 2D respeita o `Z` do Transform + a **SortingLayer**:
camadas maiores na frente, e dentro da mesma camada, `Z` menor desenha
atrás.
:::
