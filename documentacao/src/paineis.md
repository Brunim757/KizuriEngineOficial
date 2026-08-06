---
title: Painéis do editor
group: Editor
order: 2
---

# Painéis do editor

Todos os painéis podem ser abertos/fechados pelo menu **Janelas**. Você pode
arrastá-los e agrupá-los em abas do jeito que preferir.

## Viewport

O coração do editor: mostra a cena ao vivo.

- Navegação 3D: **botão direito + WASD**, **Q/E** sobe/desce, **Shift** acelera
- 2D: botão direito arrasta (pan), scroll dá zoom
- **Gizmos**: teclas **W** (mover), **E** (rotacionar), **R** (escalar)
- **F11** deixa o viewport em tela cheia (esconde os painéis)
- Barra de ferramentas no topo: modo 2D/3D, Play/Stop, stats

## Game View

Mostra o **jogo** como o jogador vai ver, durante o **Play**. É renderizado em
um framebuffer separado — você pode editar a cena no viewport enquanto observa
o resultado da câmera do jogo aqui.

## Hierarquia

A árvore de entidades da cena, com busca por nome. Clique para selecionar,
arraste para reordenar/parentar, botão direito para ações (duplicar, salvar
prefab, excluir).

## Inspetor

Mostra os **componentes** da entidade selecionada: Transform, MeshRenderer,
física, luz, câmera, script, etc. Use **Adicionar Componente** para novos.
Toda edição aqui é desfazível (**Ctrl+Z**).

## Content Browser

Navega os arquivos do projeto. Clique com o botão direito para criar pastas,
scripts, cenas e prefabs. **Arraste um asset para o viewport** (ou para os
slots do Inspetor) para usá-lo.

## Console

Logs do editor e dos scripts, com filtros por tipo (info/aviso/erro) e busca.
Clique em uma mensagem para limpar ou acompanhar a execução.

## Profiler

Acompanhe o desempenho em tempo real: **FPS**, **tempo de frame** (gráfico),
**draw calls**, **triângulos** e **entidades**. Ótimo para encontrar gargalos.

## Material Editor

Selecione uma entidade com malha e ajuste o material aqui: albedo, metal,
rugosidade, emissão, POM e espelho — com **preview ao vivo em uma esfera**.

## Animator

Para entidades com **Animador**: escolha o clip, play/pause, loop, velocidade
e arraste a linha do tempo para pular para qualquer pose.

## Project Settings

Configurações do projeto em uma janela com seções:

- **Gráficos** — preset, MSAA, sombras, reflexos, iluminação, pós-processamento e HDRI do céu
- **Geral** — resolução da janela, VSync, informações do OpenGL
- **Editor** — velocidade da câmera, sensibilidade, snapping dos gizmos
- **Sobre** — versão e resumo

::: dica Dicas rápidas
- Menu **Janelas** liga/desliga qualquer painel (o botão **X** da janela também).
- Arraste a borda de uma aba para criar uma nova coluna/linha de painéis.
- O layout fica salvo entre sessões.
:::
