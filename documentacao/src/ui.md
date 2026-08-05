---
title: UI (Canvas/Rect/Button/Text)
group: Componentes
order: 13
---

# Interface de usuário (UI)

UI desenhada por último, em **espaço de tela** (0,0 = centro, y para cima), sob
um **Canvas**. A ordem de renderização da engine é `3D → 2D → UI`, então a UI
fica por cima de tudo.

## O que há

| Componente | Papel |
|------------|-------|
| **UICanvas** | Raiz que renderiza os **descendentes** com UIRect em espaço de tela |
| **UIRect** | Retângulo colorido (fundo) |
| **UIButton** | UIRect que responde a hover/clique |
| **UIText** | Texto de UI |

## Como montar

1. Crie uma entidade e adicione **UICanvas** (`OrthoSize` = meia-altura; ex. 10);
2. Crie filhos (entidades parentadas ao canvas) com **UIRect / UIButton /
   UIText**;
3. Leia o estado dos botões no script.

```csharp
var canvas = Scene.CreateEntity("Canvas");
canvas.AddUICanvas(10f);

var botao = Scene.CreateEntity("Botão");
botao.SetParent(canvas);
botao.AddUIButton(0f, -3f, 5f, 1.2f, 0.85f, 0.25f, 0.3f); // x, y, w, h, cor
botao.AddUIText("Jogar", 0.6f, 1f, 1f, 1f);

var fundo = Scene.CreateEntity("Fundo");
fundo.SetParent(canvas);
fundo.AddUIRect(0f, 0f, 20f, 20f, 0.05f, 0.06f, 0.1f, 0.9f);
```

## Detecção de clique

```csharp
public override void OnUpdate(float deltaSeconds)
{
    if (botao.UIButtonWasClicked())
        Log.Info("Botão clicado!");

    if (botao.UIButtonIsHovered())
        // feedback visual
        botao.SetUIColor(1f, 1f, 1f, 1f);
    else
        botao.SetUIColor(0.85f, 0.25f, 0.3f, 1f);
}
```

## Coordenadas

- **0,0** = centro da tela; **y para cima**;
- **OrthoSize** = meia-altura em unidades de UI (com `OrthoSize=10`, a tela
  vai de −10 a +10 na vertical; a largura acompanha o aspecto);
- `UIRect.Position` é o **centro** do elemento.

## Em runtime

```csharp
botao.SetUIRect(x, y, w, h);           // reposiciona/redimensiona
botao.SetUIColor(r, g, b, a);          // muda a cor
```

::: info
**Alpha zero = só texto.** `UIRect` com `a=0` não desenha o fundo — útil para
rótulos de texto puro sobre a cena.
:::
