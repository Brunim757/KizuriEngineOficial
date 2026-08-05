---
title: Input
group: Scripting C#
order: 5
---

# Input

Leitura de teclado e mouse. Os códigos de tecla são os do **GLFW** (os mesmos
da engine), expostos como enum `Key`.

## Teclado

```csharp
// pressionado AGORA (estado contínuo) — uso em movimento
if (Input.IsKeyPressed(Key.A)) wish.X -= 1f;

// verdadeiro SÓ no frame em que a tecla desceu (edge-detect, GetKeyDown)
if (Input.IsKeyDown(Key.Space)) Pular();
```

### Enum Key

```csharp
Key.A .. Key.Z            // letras
Key.D0 .. Key.D9          // números
Key.Space, Key.Enter, Key.Tab, Key.Backspace, Key.Delete, Key.Escape
Key.Up, Key.Down, Key.Left, Key.Right        // setas
Key.F1 .. Key.F12
Key.LeftShift, Key.RightShift, Key.LeftControl, Key.LeftAlt
Key.LeftSuper, Key.RightSuper, …
Key.Minus, Key.Equal, Key.Semicolon, Key.Comma, Key.Period, Key.Slash
```

## Mouse

```csharp
if (Input.IsMouseButtonPressed(MouseButton.Left)) { }   // botão esquerdo
if (Input.IsMouseButtonPressed(MouseButton.Right)) { }  // direito
if (Input.IsMouseButtonPressed(MouseButton.Middle)) { } // meio

var mouse = Input.GetMousePosition();  // Vector2
```

## Combinações comuns

```csharp
// clicar SÓ uma vez (não repetido a cada frame segurando)
if (Input.IsMouseButtonPressed(MouseButton.Left) && !_mouseDown)
{
    _mouseDown = true;
    Atirar();
}
if (!Input.IsMouseButtonPressed(MouseButton.Left)) _mouseDown = false;
```

```csharp
// movimento com WASD + setas
var wish = Vector2.Zero;
if (Input.IsKeyPressed(Key.A) || Input.IsKeyPressed(Key.Left))  wish.X -= 1f;
if (Input.IsKeyPressed(Key.D) || Input.IsKeyPressed(Key.Right)) wish.X += 1f;
if (Input.IsKeyPressed(Key.W) || Input.IsKeyPressed(Key.Up))    wish.Y += 1f;
if (Input.IsKeyPressed(Key.S) || Input.IsKeyPressed(Key.Down))  wish.Y -= 1f;
```
