---
title: UI (API C#)
group: Scripting C#
order: 10
---

# UI — API C#

Construa menus e HUDs no código, além de no editor.

## Criar componentes de UI

```csharp
var canvas = Scene.CreateEntity("Canvas");
canvas.AddUICanvas(10f);   // tamanho do mundo de UI

var fundo = Scene.CreateEntity("Fundo");
fundo.AddUIRect(0f, 0f, 2f, 1f, 0.1f, 0.1f, 0.1f, 1f);

var botao = Scene.CreateEntity("Botao");
botao.AddUIButton(-1f, 0f, 2f, 0.5f, 0.3f, 0.6f, 1f, 1f);

var texto = Scene.CreateEntity("Titulo");
texto.AddText("Kizuri!", 32f, 1f, 1f, 1f);
```

## Callback de botão

```csharp
botao.OnButtonClick(() =>
{
    Scene.Load("Assets/Cenas/jogo.kzscene");
});
```

## Mutação em runtime

```csharp
texto.SetText("Fase 2");
texto.SetTextSize(40f);
texto.SetTextColor(1f, 0.8f, 0.2f);
fundo.SetSpriteColor(1f, 0f, 0f, 1f);
```

::: dica
A UI é desenhada por cima do mundo (3D → 2D → UI) e o clique do mouse é
entregue ao botão no Play — veja [Interface (UI)](ui.html).
:::
