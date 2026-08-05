---
title: Interface do editor
group: Editor
order: 1
---

# Interface do editor

O editor é dividido em um **dockspace** com painéis arrastáveis e
redimensionáveis. O layout padrão é:

```
┌──────────────────┬─────────────────────┬─────────────────────┐
│  HIERARQUIA      │   VIEWPORT          │   INSPETOR          │
│                  │                     │                     │
│  CONTENT         │   CONSOLE           │                     │
│  BROWSER         │                     │                     │
└──────────────────┴─────────────────────┴─────────────────────┘
```

| Painel | Função |
|--------|--------|
| **Hierarquia** | Árvore de entidades da cena + busca |
| **Inspetor** | Propriedades da entidade selecionada |
| **Viewport** | Pré-visualização em tempo real (2D/3D) |
| **Content Browser** | Arquivos do projeto, com drag & drop |
| **Console** | Logs da engine e dos scripts, com filtro |

Você pode arrastar as abas para reorganizar. O layout persiste durante a
sessão.

## Barra de menus

| Menu | Ações |
|------|-------|
| **Arquivo** | Novo/Abrir Projeto, Voltar ao Início, Exportar Jogo, Salvar Cena, Salvar Como, Carregar GameModule, Sair |
| **Editar** | Desfazer (Ctrl+Z), Refazer (Ctrl+Y), Duplicar (Ctrl+D), Excluir (Del) |
| **Cena** | Nova Cena, Abrir Cena, Cenas de demonstração 2D / 2.5D / 3D |
| **Exibir** | Viewport 2D/3D, Fullscreen do viewport (F11), Configurações (Ctrl+,) |
| **Ajuda** | Atalhos do editor |

## Toolbar do viewport

- Botões de **gizmo**: Mover / Rotacionar / Escalar (`W`/`E`/`R`);
- Alternância **2D / 3D**;
- **Play / Stop** (F5 / Shift+F5);
- Ícone de **fullscreen** (F11).

## Dicas rápidas

- **Clique** num objeto do viewport para selecionar (picking por raycast 3D /
  ponto 2D);
- **Botão direito arrastando** navega a câmera de edição;
- A tela inicial (hub) aparece ao abrir o editor ou em **Arquivo → Voltar ao
  Início**.
