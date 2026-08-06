---
title: Céu
group: Gráficos
order: 4
---

# Céu

O céu da Kizuri é **físico** por padrão: espalhamento **Rayleigh** (azul) e
**Mie** (haze e halos) com raios marchados na atmosfera. O resultado é um
pôr-do-sol realista com disco do sol, halo quente e estrelas no lado noturno.

## Céu físico (padrão)

- **Azul no zênite**, brilho laranja no horizonte (haze)
- O **disco do sol** segue a **luz direcional** da cena — onde a luz aponta, o sol aparece
- **Estrelas** aparecem suavemente no lado oposto ao sol
- **Nuvens volumétricas**: raymarch com ruído 3D, iluminadas pelo sol
  (ativável em Project Settings → Gráficos)

## Céu por HDRI

Para um ambiente específico (ou o seu próprio céu), carregue um arquivo
`.hdr`/`.exr` **equirectangular** no campo **HDRI do céu**:

1. Project Settings → Gráficos → campo **HDRI do céu**.
2. Escolha o arquivo (botão "..."). O editor converte para cubemap e rebakeia
   a iluminação (IBL).
3. Vazio = volta ao céu físico.

::: dica
O **pôr-do-sol** é gratuito: apenas deixe a luz direcional apontando baixa
(quase paralela ao chão) — o céu físico faz o resto.
:::

## God rays (luz volumétrica)

Raios de luz que "vazam" por entre obstáculos em direção ao sol, em espaço de
tela (marcha radial a partir do brilho). Ative em Project Settings → Gráficos
e ajuste a **intensidade**.

## Lens flare

O brilho do sol cria **ghosts** e um **streak** horizontal (efeito de lente).
Ative em Project Settings → Gráficos.
