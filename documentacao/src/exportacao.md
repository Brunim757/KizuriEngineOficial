---
title: Exportar o jogo
group: Distribuição
order: 1
---

# Exportar o jogo

O editor gera um **executável standalone** com a cena inicial e o assembly C#
do jogo.

## Como exportar

1. Deixe a cena inicial aberta (é ela que o jogo abre).
2. **Arquivo → Exportar Jogo…**
3. Escolha a pasta de destino.
4. O exportador gera:
   - O executável (`KizuriGame`) que abre a cena inicial;
   - O assembly C# do jogo;
   - O **Content Pack** (se ativado) com os assets ricos.

## O que o jogo roda

- Abre a **cena inicial** e roda o **GameModule** registrado;
- Usa a **câmera da própria cena** — o que você viu no Play é o que sai;
- Física, scripts, partículas e áudio rodam de verdade.

## Distribuição

- **Sem content**: o binário já embute os assets padrão (`kzres://`) — o jogo
  funciona sozinho, só com a cena e o assembly;
- **Com content**: copie a pasta `content/` junto para as demos/assets ricos.

::: ok
Como os padrões são embutidos, um jogo que só usa builtins e `kzres://`
distribui um **único executável** (mais a DLL do jogo).
:::

## Checklist final

- [ ] Cena inicial salva e com a câmera primária configurada
- [ ] GameModule registra todos os scripts usados
- [ ] Testou o Play de ponta a ponta
- [ ] (Opcional) Content Pack junto para os assets ricos
