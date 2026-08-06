---
title: Interface do editor
group: Editor
order: 1
---

# Interface do editor

O editor é organizado em **painéis** que você pode mover, agrupar em abas e
mostrar/ocultar pelo menu **Janelas** (na barra de menus).

## Visão geral

| Painel | Para que serve |
|---|---|
| **Viewport** | A cena em 3D/2D, com gizmos de mover/rotacionar/escalar |
| **Game View** | Mostra o jogo rodando durante o Play (separado do viewport de edição) |
| **Hierarquia** | A lista de todas as entidades da cena, com busca |
| **Inspetor** | Propriedades e componentes da entidade selecionada |
| **Content Browser** | Arquivos do projeto (assets, scripts, cenas) |
| **Console** | Mensagens, avisos e erros do editor e dos scripts |
| **Profiler** | FPS, tempo de frame, draw calls e triângulos em gráfico |
| **Material Editor** | Ajustar material com preview ao vivo em uma esfera |
| **Animator** | Clips de animação: play/pause, loop, velocidade e timeline |
| **Project Settings** | Configurações do projeto: gráficos, janela e editor |

## Barra de menus

- **Arquivo** — projetos, cenas, exportar jogo, voltar ao Hub
- **Editar** — desfazer/refazer, duplicar/excluir
- **Cena** — salvar/abrir, cenas de demonstração, modo 2D/3D
- **Exibir** — modo do viewport (2D/3D), fullscreen, Configurações
- **Janelas** — mostrar/ocultar cada painel
- **Ajuda** — versão, atalhos

## Fluxo típico

1. **Hierarquia** seleciona a entidade.
2. **Viewport** mostra e edita (gizmos).
3. **Inspetor** ajusta componentes.
4. **Content Browser** traz assets para a cena (arraste e solte).
5. **Play** testa; **Console** e **Profiler** acompanham o desempenho.

Veja os detalhes de cada painel em [Painéis do editor](paineis.html).
