---
title: Instalação
group: Introdução
order: 2
---

# Instalação

A Kizuri Engine é entregue como um pacote pronto para usar (Windows e Linux).
Não há instalação de dependências — tudo vem junto.

## Requisitos mínimos

- **GPU** com suporte a **OpenGL 3.3** (qualquer placa dos últimos ~15 anos)
- **Windows 10/11** ou **Linux**
- **4 GB de RAM** (recomendado 8 GB)
- Para scripts C#: **.NET SDK 8+** (o editor compila seu jogo no Play)

::: nota O motor funciona até em máquinas modestas
O OpenGL 3.3 roda em iGPU, máquina virtual e até Wine. Se o editor abrir, ele roda.
:::

## Como executar

1. Extraia o pacote em uma pasta de sua preferência.
2. Execute o **KizuriEditor** (`.exe` no Windows, binário no Linux).
3. Na tela inicial (Hub), crie um novo projeto ou abra um recente.

## Scripting C# (opcional no início)

Você pode fazer muito no editor sem escrever código. Quando quiser criar
lógica de jogo (movimento, pontuação, respawn...), instale o **.NET SDK** e
o editor cuidará do resto — no **Play**, ele compila seu código automaticamente.

## Solução de problemas

| Problema | Causa provável | Solução |
|---|---|---|
| Janela preta ou mensagem de erro no início | GPU sem OpenGL 3.3 | Atualize o driver de vídeo |
| Play não compila | .NET SDK ausente | Instale o .NET SDK 8+ |
| Jogo lento | Preset de qualidade alto para a máquina | Baixe o preset em Project Settings → Gráficos |

Precisa de mais ajuda? Veja as [Configurações](configuracoes.html) e a seção de
[Gerenciamento de projetos](projeto.html).
