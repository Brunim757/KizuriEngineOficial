---
title: Reflexos
group: Gráficos
order: 2
---

# Reflexos

A engine tem dois sistemas de reflexo: **SSR** (reflexos em espaço de tela)
para qualquer superfície brilhante e **espelhos planares** para superfícies
planas perfeitas.

## SSR — reflexos em espaço de tela

O SSR **marcha um raio** do pixel refletido contra o que já está na tela e
usa a cor do impacto como reflexo. Funciona em qualquer superfície metálica
ou brilhante (pisos polidos, carros, água) e usa o **Fresnel** — ângulos
rasantes refletem mais.

- Ajustes: **passos** (qualidade do raio), **intensidade**, **distância** e
  **espessura**
- Meia/total resolução conforme o preset

::: nota
O SSR é implementado com loop de passos **fixos**, então funciona em
qualquer GPU OpenGL 3.3 — sem os bugs de drivers 4.x.
:::

## Reflexões planares (espelho)

Para um espelho **perfeito** (sem limite de ângulo), marque **Reflexão planar**
no material (Inspetor ou Material Editor). O editor renderiza a cena de novo
de uma **câmera refletida** e a superfície usa essa imagem:

1. Crie um plano e aplique um material escuro/metálico.
2. No material, marque **Reflexão planar (espelho)**.
3. Pronto — o plano reflete a cena como um espelho real.

::: dica
Use SSR para superfícies brilhantes em geral e **espelho planar** apenas
onde precisar de precisão (espelho de banheiro, chão de salão). O espelho
planar custa uma renderização extra da cena.
:::
