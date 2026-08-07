---
title: Céu
group: Gráficos
order: 4
---

# Céu

O céu da Kizuri tem três modos, em ordem de prioridade:

1. **HDRI** — se você carregar um `.hdr`/`.exr`
2. **Atmosférico Rayleigh/Mie** — se o modo físico estiver **ligado** (opcional)
3. **Gradiente procedural** — o padrão (limpo, leve e estável em qualquer GPU)

## Gradiente procedural (padrão)

O céu padrão é um **gradiente procedural** (azul no zênite, brilho de
pôr-do-sol no horizonte, estrelas à noite), gerado e embutido na engine.
É leve e estável até em GPUs fracas/emuladores.

## Céu atmosférico Rayleigh/Mie (opcional)

Para um céu **físico** com espalhamento de luz real (pôr-do-sol laranja,
halo do sol, estrelas profundas), ligue a opção **"Céu atmosférico
Rayleigh/Mie"** em Configurações → Gráficos (ou Project Settings → Gráficos).

::: aviso
O Rayleigh/Mie é um **raymarch** pesado — pode gerar pontilhado/lentidão em
GPUs fracas e emuladores (ex.: Winlator). Deixe **desligado** neles.
:::

- O **disco do sol** segue a **luz direcional** da cena — onde a luz aponta, o sol aparece
- **Nuvens volumétricas**: camada suave iluminada pelo sol (ligável junto)
- **Estrelas** aparecem no céu alto, no lado noturno

## Céu por HDRI

Para um ambiente específico (ou o seu próprio céu), carregue um arquivo
`.hdr`/`.exr` **equirectangular** no campo **HDRI do céu**:

1. Project Settings → Gráficos → campo **HDRI do céu**.
2. Escolha o arquivo (botão "..."). O editor converte para cubemap e rebakeia
   a iluminação (IBL).
3. Vazio = volta ao gradiente procedural.

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
