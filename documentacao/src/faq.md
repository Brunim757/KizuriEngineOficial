---
title: Perguntas frequentes
group: Distribuição
order: 3
---

# Perguntas frequentes

## O jogo roda em qualquer PC?

A engine usa **OpenGL 3.3** — roda em qualquer placa de vídeo dos últimos
~15 anos, incluindo iGPUs, máquinas virtuais e até Wine. Se o editor abrir,
seu jogo roda.

## Preciso saber C++ para fazer um jogo?

Não. O motor é em C++, mas você usa o **editor** e o **C#**. Tudo o que o
jogo faz passa por scripts C# com API simples.

## Consigo fazer jogos 2D e 3D?

Sim, na mesma cena (2.5D). Câmera ortográfica para o 2D, perspectiva para o
3D, e a engine compõe 3D → 2D → UI na ordem certa.

## Como fica o desempenho com todos os efeitos ligados?

O preset **Ultra** liga tudo (reflexos, SSGI, nuvens, DOF...). Se precisar de
mais FPS, desça o preset ou desligue os efeitos mais caros (MSAA, SSGI, SSR)
em **Project Settings → Gráficos**. O **Profiler** mostra onde está o custo.

## Posso usar meus próprios modelos e texturas?

Sim. **glTF (.glb/.gltf)** traz malha, material PBR e animações; **OBJ**
para geometria simples; e imagens comuns para texturas. Arraste do Content
Browser para o viewport.

## Meu jogo trava no Play. O que faço?

1. Olhe o **Console** — os erros do script aparecem lá.
2. Verifique se há **câmera principal** na cena (sem ela o 3D não renderiza).
3. Verifique se o **.NET SDK** está instalado (o Play compila seu código).
4. Se o viewport ficar preto, o diagnóstico aparece em `render_info.txt` ao
   lado do editor.

## Como distribuo meu jogo?

**Arquivo → Exportar Jogo...** gera uma pasta executável autocontida. Basta
compactar e distribuir.

## Onde fica o código do meu jogo?

Em `Source/` dentro do projeto (arquivos `.cs`), compilados no Play e no
export para `Kizuri.Scripting.dll`.
