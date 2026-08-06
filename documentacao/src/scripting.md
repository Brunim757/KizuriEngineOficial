---
title: Scripting C#
group: Scripting C#
order: 1
---

# Scripting C#

O jogo é um assembly **C#** (`Kizuri.Scripting`): você escreve scripts, o
editor compila no **Play** e o motor conversa com seu código por uma API
estável. Você nunca mexe no C++ da engine.

## O ciclo

1. Crie um script no Content Browser (botão direito → **Novo Script C#**).
2. Escreva a lógica herdando de `KizuriScript`.
3. Anexe o script a uma entidade (Inspetor → Adicionar Componente → Script C#).
4. Aperte **Play** — o editor compila e roda.

## Estrutura de um script

```csharp
using Kizuri;

public class MeuScript : KizuriScript
{
    // roda uma vez, quando a entidade entra na cena
    public override void OnStart() { }

    // roda a cada frame
    public override void OnUpdate()
    {
        // entidade atual, cena, tempo...
        Entity.Move(1f, 0f, 0f);
        if (Input.IsKeyDown(KeyCode.Space)) Scene.Destroy(Entity);
    }

    // opcional: callbacks de colisão
    public override void OnCollisionBegin(Entity other) { }
}
```

## O que a API oferece

- **Entity** — criar, destruir, transform, sprites, texto, áudio, malhas, física, animação
- **Scene** — criar entidades, prefabs, trocar cena, raycasts
- **Input** — teclado e mouse
- **Time** — deltaTime, escala de tempo
- **Mathf / Rand / Vectors** — matemática de jogo
- **SaveSystem** — persistência em JSON
- **UI** — canvas, botões e callbacks

## Convenções

- Tudo é **público e simples**: nada de ponteiros, nada de alocação manual.
- O namespace é `Kizuri`.
- C# moderno (.NET 8+): async/await, LINQ, etc. funcionam.

Continue em [Entity](entity.html) para a API completa.
