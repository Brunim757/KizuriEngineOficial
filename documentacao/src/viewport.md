---
title: Viewport
group: Editor
order: 3
---

# Viewport

O painel **Viewport** é onde você vê e edita a cena.

## Navegação 3D

| Ação | Controle |
|---|---|
| Olhar ao redor | Botão direito + mover o mouse |
| Mover | Botão direito + **WASD** |
| Subir / descer | Botão direito + **Q / E** |
| Acelerar | Segurar **Shift** enquanto move |
| Foco | Selecione na Hierarquia e use o gizmo |

## Navegação 2D

- **Botão direito + arrastar** move a câmera (pan)
- **Scroll** dá zoom
- O zoom centraliza no ponteiro do mouse

## Gizmos

Com uma entidade selecionada, use as teclas:

- **W** — mover (setas de translação)
- **E** — rotacionar (esferas)
- **R** — escalar (cubos)

Segure **Ctrl** durante o arrasto para **snap** (incrementos de 0,5 unidade,
15° de rotação — configuráveis em Project Settings → Editor).

## Modo 2D / 3D

A barra de ferramentas do viewport alterna entre os modos:

- **2D** — câmera ortográfica, ideal para sprites, tilemaps e UI
- **3D** — câmera em perspectiva, para meshes e luzes

O editor muda automaticamente o modo conforme o componente selecionado
(malha 3D → 3D; sprite → 2D).

## Fullscreen

- Botão de ícone na toolbar ou **F11** esconde os painéis laterais e o
  viewport ocupa a janela inteira — bom para jogar/testar.

## Seleção

- Clique em um objeto no viewport para selecionar (raio contra as malhas).
- Selecione na **Hierarquia** e pressione **F** (ou use o gizmo) para focar.

::: nota
O viewport mostra a cena **de edição**; o **Game View** mostra a cena do
jogo durante o Play.
:::
