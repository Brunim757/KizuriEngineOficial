---
title: Input (API)
group: Scripting C#
order: 4
---

# Input — API

Leitura de **teclado** e **mouse** para controlar o jogo.

## Teclado

```csharp
if (Input.IsKeyDown(Key.Space)) Pular();
if (Input.IsKeyPressed(Key.Left)) MoverEsquerda();
```

## Mouse

```csharp
if (Input.IsMouseButtonPressed(MouseButton.Left)) Atirar();          // 0 = esquerdo, 1 = direito
if (Input.IsMouseButtonPressed(MouseButton.Left)) Salto(); // só no frame do clique
var (x, y) = Input.GetMousePosition();          // posição em pixels na janela
```

## Eixo simples

Para movimento, combine teclas:

```csharp
float mover = 0f;
if (Input.IsKeyDown(Key.A)) mover -= 1f;
if (Input.IsKeyDown(Key.D)) mover += 1f;
Entity.MoveRight(mover * deltaSeconds * velocidade);
```

## Input Actions (com rebind)

Mapeie um **nome de ação** a uma tecla e use o nome no código — o jogo deixa
de depender da tecla física e o jogador pode rebindar:

```csharp
// a partir do rebind (ou no início do jogo):
Input.SetActionKey("Pular", Key.Space);

// usar sempre pelo nome:
if (Input.IsActionPressed("Pular")) Pular();
if (Input.IsActionPressed("Esquerda")) Mover(-1f);
```

Ações padrão já existem: `Pular`, `Esquerda`, `Direita`, `Cima`, `Baixo`,
`Acao`, `Correr`, `Cancelar`.

## Códigos comuns

| Código | Tecla |
|---|---|
| `Key.Space` | Espaço |
| `Key.W/A/S/D` | WASD |
| `Key.Up/Down/Left/Right` | Setas |
| `Key.Enter` / `Key.Escape` | Enter / Esc |
| `Key.Shift` / `Key.Ctrl` | Shift / Ctrl |

::: nota
O mouse do **Play** acompanha o viewport (e o Game View) — os cliques de UI
e do jogo funcionam na janela do editor.
:::
