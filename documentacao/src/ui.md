---
title: Interface (UI)
group: Componentes
order: 10
---

# Interface (UI)

A UI da Kizuri é baseada em **canvas + retângulos + botões + texto**, em
espaço de tela — menus, HUDs e barras de vida.

## Componentes

| Componente | O que faz |
|---|---|
| **UICanvas** | A "folha" onde a UI vive (tamanho do mundo de UI) |
| **UIRect** | Retângulo colorido (fundo de botão, barra) |
| **UIButton** | Botão clicável com callback |
| **UIText** | Texto em tela |

## Como montar um menu

1. Crie uma entidade com **Canvas**.
2. Filhos dela com **Rect** (cores) e **Botão**.
3. No botão, defina o callback no script:

```csharp
public sealed class Menu : Script
{
    public override void OnCreate()
    {
        var botao = Entity; // entidade do botão
        botao.OnButtonClick(() =>
        {
            Scene.Load("Assets/Cenas/jogo.kzscene");
        });
    }
}
```

## Dicas

- A UI é desenhada **por cima** do mundo 3D/2D (ordem 3D → 2D → UI).
- No **Play**, o mouse da UI acompanha o viewport (hit-test de botões).
- Combine com **câmera 2D principal** para HUDs sobre um mundo 3D (2.5D).

::: dica
Veja a [API de UI](ui-api.html) para todos os controles e callbacks.
:::
