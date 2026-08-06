---
title: Pós-processamento
group: Gráficos
order: 6
---

# Pós-processamento

Toda a "pós-cena" roda em passes de tela cheia depois de renderizar a
geometria — tudo em OpenGL 3.3.

## Bloom

Brilho extra em superfícies emissivas e no sol (um bright-pass borrado somado
à cena). Ajustes: **limiar**, **intensidade** e **iterações** (glow mais largo).

**Bloom anamórfico** alonga o blur horizontal — os highlights ganham "streaks"
de cinema.

## Anti-aliasing

- **TAA** (temporal): o mais eficaz — desloca a câmera meio pixel por frame
  (sequência Halton) e mistura com o histórico, com clamp de vizinhança para
  não borrar. Remove "estrelinhas" das silhuetas.
- **FXAA**: pós-processamento simples, ótimo como reforço em cima do TAA.

## Depth of field (bokeh)

Desfoca o que está fora do plano focal, com **bokeh** (disco de Poisson).
Ajustes: **distância focal**, **faixa em foco** e **força** do desfoque.

## Motion blur

Borrado de movimento por **reprojeção**: o ponto é reconstruído no depth e
projetado com a câmera do frame anterior — o blur segue a direção do
movimento real. Ajuste a **intensidade**.

## Exposição e tonemapping

- **Exposição** — brilho geral (antes do tonemap).
- **Tonemapping**: **ACES** (cinematográfico), **Reinhard** (suave) ou
  **Filmic** (contraste).

## Color grading

**Saturação** e **contraste** aplicados depois do gamma — para dar identidade
visual (menos saturado = mais sério, mais quente = mais acolhedor).

## Filmico

- **Vinheta** — escurece as bordas
- **Aberração cromática** — separa R/G/B nas bordas (lente)
- **Grão de filme** — textura animada de filme

## Névoa (fog)

Névoa exponencial por distância. Com **altura** e **atenuação por altura**, a
névoa fica forte no chão e some para o alto — ótimo para névoa de vale.

::: dica
Combinação clássica de "cinema": TAA + bloom anamórfico leve + vinheta +
grão sutil. Comece com os valores do preset **Ultra** e ajuste devagar.
:::
