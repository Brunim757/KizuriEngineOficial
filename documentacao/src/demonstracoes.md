---
title: Demonstrações
group: Editor
order: 9
---

# Demonstrações

O editor vem com **cenas de demonstração** prontas — uma para cada recurso
da engine. Elas mostram como montar cada sistema na prática e servem de
ponto de partida pra copiar configurações pro seu jogo.

**Menu:** Cena → Demonstrações

## Demos disponíveis

| Demo | O que mostra |
|------|--------------|
| **2D** | Sprites, física Box2D (caixas que caem), moedas circulares, texto, UI Canvas com botão |
| **3D** | Malha PBR com esqueleto animado (Fox), luzes, céu físico/HDRI, reflexos |
| **IA** | NavGrid, patrulha/perseguição com EnemyAI e NavAgent |
| **Rede** | Multiplayer local (Host/Connect), sincronização de transform via rede |
| **Jogo completo** | Mini-jogo fechado: colete 8 moedas fugindo dos inimigos, placar, WASD |

::: dica
As demos são o melhor atalho pra aprender: abra uma, entre no Play,
depois inspecione cada entidade pra ver como os componentes estão
configurados.
:::

## Como as demos são organizadas

Cada demo é gerada por código dentro do editor (arquivo `DemoScenes.cpp`)
— não são arquivos `.kzscene` salvos. Isso garante que a demo sempre usa
os valores padrão atualizados da engine, sem depender de arquivos antigos.

Adicionar uma demo nova = uma função nova nesse arquivo + um item no
submenu Cena → Demonstrações. Uma demo por recurso da engine é a meta:
física de veículos, tilemap, chunked world, partículas avançadas etc.
terão as suas.

## Fluxo recomendado

1. Abra a demo do recurso que você quer aprender (ex.: IA).
2. Aperte **Play** e interaja.
3. Pare o Play e explore o Inspetor das entidades envolvidas.
4. Copie os componentes pro seu próprio jogo.