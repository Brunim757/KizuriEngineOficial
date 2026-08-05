---
title: Assets padrão
group: Conceitos
order: 3
---

# Assets padrão

O editor **já vem com tudo pronto** para você começar a criar — sem baixar
nada separado.

## O que já vem junto

- **Modelos 3D** — cubo, plano, esfera, cilindro, cone, cápsula e toro
  (gerados pelo motor);
- **Cenas de demonstração** — 2D, 2.5D e 3D (com personagem animado e
  objetos PBR), prontas no menu **Cena**;
- **Céu** — HDRI padrão;
- **Fonte** — a fonte de texto dos HUDs já está incluída;
- **Textura de partículas** — o efeito de partículas funciona sem textura
  (usa um brilho suave gerado pelo motor).

## Caminhos prontos

Alguns campos de asset aceitam caminhos especiais que **sempre funcionam**:

| Caminho | O que é |
|---------|---------|
| `builtin:cube` / `builtin:plane` / `builtin:sphere` / … | Malhas geradas pelo motor |
| `kzres://…` | Asset padrão do editor (ex.: `kzres://models/Fox.glb`) |

::: ok
**Nada quebra se você apagar um arquivo.** Os padrões usados pelas funções e
demonstrações não dependem de arquivos soltos — remova o que quiser que as
funções continuam funcionando.
:::

## Seus próprios assets

Seus arquivos (modelos `.obj`/`.glb`, imagens, sons) ficam na pasta do seu
projeto e são vistos no [Content Browser](content-browser.html). O Content
Pack da distribuição só **enriquece** as demos com assets extras — nunca é
obrigatório.
