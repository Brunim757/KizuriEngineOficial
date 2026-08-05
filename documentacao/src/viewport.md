---
title: Viewport e câmera
group: Editor
order: 2
---

# Viewport e câmera

O **Viewport** é a pré-visualização em tempo real da cena. Ele tem dois modos
de navegação do editor (independentes da câmera do jogo).

## Modos 2D e 3D

- **3D** — navegação orbital: **botão direito arrastando** gira a câmera de
  edição; a roda do mouse aproxima/afasta.
- **2D** — arraste com **botão direito** para pan e use a **roda** para zoom.
  A câmera de edição é ortográfica, alinhada ao plano XY.

Alternne pela toolbar do viewport ou pelo menu **Exibir**.

## Câmera do jogo

No **Play** (e no jogo exportado), quem renderiza é a **câmera da própria
cena** — não a câmera de navegação do editor. O que você vê no Play é o que
sai no build.

Veja o componente [Câmera](camera.html) para configurar perspectiva vs.
ortográfica, FOV, near/far e qual câmera é a primária.

## Fullscreen

<kbd>F11</kbd> (ou o ícone da toolbar) maximiza o viewport para a janela
inteira — útil para testar a cena "de verdade".

## Picking

- **Clique esquerdo** em um objeto seleciona a entidade.
- Em 3D, a seleção usa **raycast** contra as meshes (mesh picking).
- Em 2D, a seleção usa o ponto de clique sobre os sprites.

## Gizmos

Com uma entidade selecionada, aparecem os gizmos:

| Tecla | Gizmo | Ação |
|-------|-------|------|
| <kbd>W</kbd> | Mover | Arraste as setas dos eixos |
| <kbd>E</kbd> | Rotacionar | Arraste os anéis |
| <kbd>R</kbd> | Escalar | Arraste as alças |

Luzes e colliders também têm gizmos próprios (posição/direção da luz, e o
contorno do colisor na entidade selecionada).
