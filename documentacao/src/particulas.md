---
title: Partículas
group: Componentes
order: 6
---

# Partículas

O **ParticleSystem** cria partículas (fumaça, faíscas, poeira, fogo...) com
um único draw call instanciado.

## Como usar

1. Adicione **Partículas** à entidade.
2. Ajuste o comportamento no Inspetor.
3. No script, use `Spawn` para emitir (ou deixe a emissão contínua).

## O que dá para controlar

- **Posição/velocidade** inicial (e aceleração)
- **Vida útil** de cada partícula
- **Tamanho** (inicial → final) e **cor** (inicial → final)
- **Textura** (opcional; sem textura = quad colorido)
- **Additive** — partículas aditivas (fogo/brilho) ou normais (fumaça)

## Dicas

- Partículas são renderizadas com **instancing** — milhares de partículas
  custam pouco.
- Use **additive** para brilho e fogo; **normal** para fumaça e poeira.
- Partículas testam a profundidade contra a geometria (ficam atrás de
  paredes) mas não se ocultam entre si.

::: dica
Combine com **emissivo + bloom** para faíscas que "brilham" — veja
[Pós-processamento](pos-processamento.html).
:::
