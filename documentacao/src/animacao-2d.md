---
title: Animação 2D
group: Componentes
order: 12
---

# Animação 2D

A animação 2D roda **quadros de uma folha de sprite** (sprite sheet): o
componente avança automaticamente a região do atlas ao longo do tempo.

## Como usar

1. Importe uma **sprite sheet** (uma imagem com os quadros em grade).
2. No sprite/atlas, defina **colunas** e **linhas** da grade.
3. Configure a animação: **velocidade** (quadros por segundo) e **loop**.

## No script

```csharp
// tocar a animação da entidade
Entity.PlayAnimation();
Entity.SetAnimationPlaying(false);
// trocar a velocidade
Entity.SetAnimationSpeed(8f);
```

## Dicas

- Combine com **FlipX** para virar o personagem ao mudar de direção.
- Use **SortingLayer** para personagens na frente/atrás de cenários.
- Para **animação esquelética 3D**, veja [Animação 3D](animacao-3d.html).

::: dica
O viewport mostra o **preview da animação** em modo de edição — dê play e
veja o personagem andando sem apertar Play.
:::
