---
title: Perguntas frequentes
group: Distribuição
order: 2
---

# Perguntas frequentes

## A engine precisa do Content Pack para funcionar?

**Não.** Todos os padrões (modelos da demo 3D, céu, primitivas) estão
**embutidos no executável** via `kzres://`. O Content Pack apenas enriquece as
demos.

## Minha placa de vídeo é antiga. Perco alguma coisa?

Só o acabamento mais sofisticado de sombras, que exige uma placa mais nova.
Tudo o resto — luzes, céu, partículas, personagens animados, 2D e UI —
funciona em qualquer placa que rode o editor. O motor escolhe o melhor
automaticamente.

## Preciso me preocupar com placa de vídeo?

Só se ela tiver **OpenGL 3.3** ou mais (qualquer GPU razoável dos últimos
~15 anos). O motor ajusta a qualidade sozinho pelo seu hardware — em geral
você não precisa mexer em nada.

## Como escrevo scripts?

Em um projeto C# que referencia `Kizuri.Scripting`. No editor, **Arquivo →
Carregar GameModule…** aponta para a DLL. No Play, o C# é recompilado e
recarregado automaticamente.

## O que vejo no Play é o que sai no build?

**Sim.** O Play e o jogo exportado usam a câmera da própria cena, não a câmera
livre de navegação do editor.

## As cenas são salvas em quê?

JSON legível (`.kzscene`), incluindo mesh, material e texturas. Dá para editar
no bloco de notas e versionar no git.

## Rotação: graus ou radianos?

- **Inspetor**: graus.
- **API C#**: sempre **radianos** (converão `graus * MathF.PI / 180f`).

## Posso fazer 2.5D?

**Sim.** Uma cena com câmera de perspectiva **e** câmera ortográfica primária
roda os passes 3D → 2D → UI. Há uma demo pronta no menu **Cena**.

## Física em modo de edição?

Não — física, queries, partículas e áudio só rodam no **Play** / jogo
exportado. No modo de edição os corpos ficam parados no Transform.

## Como troco de cena em runtime?

`Scene.Load("Assets/Fase2.kzscene")` — o pedido é processado no fim do frame.

## Como instancio uma prefab?

`Scene.InstantiatePrefab("Assets/Inimigo.kzprefab", posicao, rotacao)` — a
física e o `OnCreate` dos scripts rodam de verdade.

## Como salvo o jogo?

`SaveSystem.Set("chave", valor)` + `SaveSystem.Save()` grava um `save.json`;
os `Get*` carregam automaticamente.

## Consigo misturar 2D e 3D na mesma cena?

Sim — cenas **2.5D** (fundo 3D, gameplay 2D, UI por cima) funcionam com duas
câmeras. Há uma demonstração pronta no menu **Cena**.
