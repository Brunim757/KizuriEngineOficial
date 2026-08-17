---
title: Scripting C#
group: Scripting C#
order: 1
---

# Scripting C#

O jogo é um assembly **C#**: você escreve scripts em `Source/` do seu
projeto, o editor compila no **Play** (ou no export) e o motor conversa com
seu código por uma API estável e pronta (`Kizuri.Scripting.dll` — entregue
compilada, fechada). Você nunca mexe no C++ da engine.

## O ciclo (simples)

1. Botão direito no Content Browser → **Criar Script C#...** (gera um
   `.cs` na pasta atual — de preferência em `Source/` do projeto).
2. Escreva a lógica herdando de `Script`.
3. Selecione a entidade → Inspetor → **+ Adicionar Componente → Script C#**
   → escolha a classe no dropdown.
4. Aperte **Play** — o editor compila e **registra a classe
   automaticamente** (qualquer classe pública `: Script` aparece no
   dropdown; não precisa de registro manual).

## Estrutura de um script

```csharp
using Kizuri;

public sealed class MeuScript : Script
{
    // roda uma vez, quando a entidade entra na cena
    public override void OnCreate() { }

    // roda a cada frame (deltaSeconds = segundos do último frame)
    public override void OnUpdate(float deltaSeconds)
    {
        Log.Info($"MeuScript na entidade '{Entity.Id}' rodando.");
        if (Input.IsKeyPressed(Key.Space))
            Entity.Destroy();
    }

    public override void OnCollisionBegin(Entity other) { }
    public override void OnCollisionEnd(Entity other) { }
    public override void OnDestroy() { }
}
```

## O que a API oferece

- **Entity** — nome, ativa/desativa, transform (`SetPosition`, rotação,
  escala), criar sprites/texto/áudio/malhas/física, `Destroy()`, componentes
- **Scene** — `CreateEntity`, `InstantiatePrefab`, `Load` (troca de cena),
  `Find(name)`, `GetPrimaryCamera()`, `EntitiesWithTag`
- **Input** — `IsKeyPressed(Key)`, `IsKeyDown(Key)`, `IsMouseButtonPressed`,
  `GetMousePosition()`, ações (`IsActionPressed`, `SetActionKey`)
- **Time** — deltaTime, tempo acumulado, escala de tempo
- **Mathf / Rand / Vectors** — matemática de jogo
- **SaveSystem** — persistência em JSON (`SaveSystem.SetFloat/GetFloat`)
- **Audio / Network / Coroutine** — som, multiplayer e loops
- **UI** — canvas, botões e callbacks

## Convenções

- Namespace `Kizuri`; teclas são `Key.Space`, `Key.A`, `Key.Left` etc.
  (enums `Key`/`MouseButton`).
- C# moderno (.NET 10): `async/await`, LINQ etc. funcionam.
- O registro manual ainda existe (`[Kizuri.GameEntryPoint]` +
  `Kizuri.GameModule.Register<N>("Nome")`) para quem quiser nomes
  customizados — o automático tem prioridade menor.

Continue em [Entity](entity.html) para a API completa.