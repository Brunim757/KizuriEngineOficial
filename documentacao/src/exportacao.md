---
title: Exportar o jogo
group: Distribuição
order: 1
---

# Exportar o jogo

O menu **Arquivo → Exportar Jogo...** gera uma pasta com o **jogo
standalone**: executável + cena + assets + seu código compilado.

## Como exportar

1. Tenha uma cena salva e defina-a como **cena inicial** (Arquivo → Definir
   cena como inicial).
2. **Arquivo → Exportar Jogo...**
3. Preencha as **build settings**: nome do jogo, versão e resolução da janela
   (ficam salvas no projeto).
4. Escolha a pasta de destino.
5. Opcional: marque **embutir runtime .NET** (gera o executável com o
   runtime do C# incluído — o jogador não precisa instalar nada).

## O que sai

```
MinhaPasta/
  KizuriGame.exe          ← o jogo
  cena.kzscene            ← cena inicial
  assets/...              ← modelos, texturas, áudios
  Kizuri.Scripting.dll    ← seu código C# compilado
```

## Recursos embutidos

A engine embute os **assets padrão** (`kzres://`) no executável: modelos de
demonstração, céu e fontes. Isso significa que o jogo não depende de pastas
externas para as funções básicas — e você pode embutir os seus assets também.

## Publicação

- A pasta exportada é autocontida: copie para qualquer máquina (com GPU
  OpenGL 3.3) e rode.
- Para distribuir para lojas ou sites, compacte a pasta em um `.zip`.

## Releases (GitHub)

A engine é publicada como **Release do GitHub**: ao criar uma tag `vX.Y.Z`
e enviá-la (`git tag v0.36.0 && git push origin v0.36.0`), o CI compila e
publica os pacotes: executáveis Windows/Linux + assembly C# + Content Pack.

::: dica
Teste o export cedo: defina a cena inicial, exporte e rode o executável.
Só a cena inicial é carregada automaticamente — o resto é carregado por
`Scene.Load` no jogo.
:::
