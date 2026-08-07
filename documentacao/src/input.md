---
title: Input (API)
group: Scripting C#
order: 4
---

# Input — API

Leitura de **teclado** e **mouse** para controlar o jogo.

## Teclado

```csharp
if (Input.IsKeyDown(KeyCode.Space)) Pular();
if (Input.IsKeyPressed(KeyCode.Left)) MoverEsquerda();
```

## Mouse

```csharp
if (Input.IsMousePressed(0)) Atirar();          // 0 = esquerdo, 1 = direito
var (x, y) = Input.GetMousePosition();          // posição em pixels na janela
```

## Eixo simples

Para movimento, combine teclas:

```csharp
float mover = 0f;
if (Input.IsKeyDown(Key.A)) mover -= 1f;
if (Input.IsKeyDown(Key.D)) mover += 1f;
Entity.Move(mover * Time.deltaTime * velocidade, 0f, 0f);
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
| `KeyCode.Space` | Espaço |
| `KeyCode.W/A/S/D` | WASD |
| `KeyCode.Up/Down/Left/Right` | Setas |
| `KeyCode.Enter` / `KeyCode.Escape` | Enter / Esc |
| `KeyCode.Shift` / `KeyCode.Ctrl` | Shift / Ctrl |

::: nota
O mouse do **Play** acompanha o viewport (e o Game View) — os cliques de UI
e do jogo funcionam na janela do editor.
:::
