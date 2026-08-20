---
title: Chunked World
group: Mundo / Streaming
order: 1
---

# Chunked World

O **Chunked World** divide o mundo do jogo em uma grade de pedaços
(chunks) que são **carregados e descarregados por distância** até um
ponto de referência (o jogador ou a câmera). É o primeiro passo para
mundos grandes: a cena não precisa mais ter tudo na memória de uma vez.

Funciona em jogos **2D ou 3D** — o que importa é a posição do alvo no
plano XZ.

## Como funciona

- A cena tem uma entidade com o componente **Chunk World** (a "cena
  base" — luz, câmera, jogador, sistemas).
- Os chunks são arquivos `.kzchunk` guardados numa pasta do projeto
  (padrão `Assets/Chunks/`), um por célula da grade: `chunk_0_0.kzchunk`,
  `chunk_1_-2.kzchunk`...
- Em runtime, a engine calcula a grade de chunks ao redor do alvo e
  **carrega até 2 chunks por frame** (sem travar), descarregando os que
  ficaram distantes.

## Configurando

1. Crie uma entidade na cena e adicione o componente **Chunk World**.
2. Ajuste no Inspetor:
   - **Tamanho do chunk (m)**: quantos metros (X/Z) cada chunk cobre
   - **Raio de carga**: quantos chunks ao redor do alvo ficam carregados
   - **Folga antes de descarregar**: chunks extras que continuam
     carregados ao redor (evita descarregar/carregar em flicka ao
     atravessar uma borda de chunk)
   - **Pasta dos chunks**: subpasta de `Assets/` onde ficam os `.kzchunk`
   - **Tag do alvo**: entidade cuja posição define o centro do carregamento
3. Monte os chunks:
   - Crie as entidades de cada região do mundo.
   - Adicione o componente **Chunk Entity** em cada entidade que pertence
     a um chunk e preencha as coordenadas **Chunk X/Z**.
   - Clique em **"Salvar chunks do mundo"** no Inspetor — a engine agrupa
     as entidades por coordenada e grava um `.kzchunk` por célula.
4. Aperte **Play** — os chunks carregam/descarregam conforme o alvo anda.

::: dica
Use **razoes pequenos de carga (1–3)** em jogos de mundo aberto: cada chunk
carregado instancia entidades reais (meshes, colisores, scripts). Quanto
menor o raio, menos trabalho por frame.
:::

::: aviso
Todo `ChunkEntityComponent` é **removido ao descarregar o chunk** — scripts
dentro do chunk também são destruídos. Mantenha o jogador, a câmera e o
som do mundo na **cena base**, fora dos chunks.
:::

## Ciclo de vida em runtime

1. O alvo entra numa célula → os chunks vizinhos entram na fila de carga.
2. Até **2 chunks por frame** são instanciados (entidades criadas).
3. O alvo se afasta → chunks fora do anel (raio + folga) são
   destruídos, junto com todas as entidades marcadas com Chunk Entity.

## Arquivos

Formato dos arquivos `.kzchunk`

```json
{
  "Chunk": { "cx": 0, "cz": 0 },
  "Entities": [ ... ]
}
```

O array `Entities` usa o mesmo formato das entidades de cena — ou seja,
você pode abrir, editar ou gerar chunks fora da engine.

## Limitações atuais

- O carregamento é **esparso**: células sem arquivo são simplesmente
  ignoradas (não há geração procedural automática).
- Os chunks são descarregados por **anéis retangulares**, não por
  distância euclidiana (também válido, só visualmente mais "quadrado").