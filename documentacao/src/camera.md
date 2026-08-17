---
title: Câmera
group: Componentes
order: 2
---

# Câmera

A **câmera** define o que o jogador vê. Um jogo pode ter várias, mas a que
tiver **Principal** marcado é a usada pelo **Play** e pelo **Game View**.

## Tipos

- **2D (Ortográfica)** — sem perspectiva, ideal para sprites e UI
- **3D (Perspectiva)** — profundidade real, FOV ajustável

## Propriedades

- **Principal** — se é a câmera principal (usada no Play)
- **FOV** (3D) — campo de visão (45° é o padrão)
- **Near / Far** — planos de recorte (o que fica entre eles é renderizado)
- **OrthoSize** (2D) — quanto do mundo aparece na tela

## Orientação (importante)

A direção da câmera vem da **matriz do Transform da entidade** — o mesmo
que o mesh usa. Com rotação `(0,0,0)` a câmera olha para **-Z** (frente),
que é onde ficam os objetos padrão das cenas de exemplo. Euler `Y=-90°`
aponta para **+X** (lado direito) — não use -90° de yaw pra olhar "pra
frente"; deixe `Y` em 0.

## Editar vendo ao vivo

- Selecione a câmera (botão **Focar câmera** no Game View faz isso) e edite
  pelo **Inspetor** — funciona **durante o Play** também.
- Ao parar o Play, os ajustes de câmera (tipo, FOV, near/far, ortho,
  primary, posição/rotação) **voltam pra cena** — não se perdem.

## Câmera do editor

Independente das câmeras da cena: voe com botão direito + WASD, **Alt** +
arrastar orbita no pivô (entidade selecionada) e `1`/`3`/`7` dão vistas
rápidas (frente/direita/topo).

::: nota
Sem câmera principal, o **Play** não renderiza nada no 3D. Se a tela ficar
vazia, adicione uma câmera e marque **Principal**.
:::
