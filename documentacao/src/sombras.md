---
title: Sombras
group: Gráficos
order: 5
---

# Sombras

A luz direcional projeta **sombras em cascata (CSM)**: três mapas de
profundidade em distâncias crescente, para sombras nítidas perto da câmera e
cobertura total ao longe.

## Ajustes

- **Tamanho do shadow map (CSM)** — 512 a 4096. Maior = sombra mais nítida,
  mais custo.
- **PCF** — amostras de suavização da borda (0 a 3).
- **Penumbra (PCSS)** — **sombras suaves de verdade**: a largura da penumbra
  é proporcional à distância do bloqueador. `0` desliga (usa PCF puro).

::: nota
O PCSS usa loops de passos **fixos** no shader — funciona em qualquer GPU
OpenGL 3.3 (a versão antiga de raio dinâmico era o que quebrava em alguns
drivers e foi substituída).
:::

## Dicas de qualidade

- **Acne** (listras na sombra): aumenta o bias via valores de material ou
  aproxime o mapa — geralmente 2048 resolve.
- **Sombras "blocadas" ao longe**: aumente o tamanho do CSM.
- **Penumbra exagerada**: reduza o valor de PCSS.

::: dica
Luzes de **ponto** e **spot** iluminam sem sombra projetada (a sombra é da
luz direcional). Para efeito de "spot com sombra", use sombras falsas
(texturas de silhueta) ou ilumine com a direcional.
:::
