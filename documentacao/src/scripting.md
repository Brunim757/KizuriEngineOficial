---
title: Introdução ao C#
group: Scripting C#
order: 1
---

# Scripting C#

A **única API pública de gameplay** é o assembly `Kizuri.Scripting`. O motor
C++ fica 100% privado: o seu jogo conversa com a engine apenas por um **ABI
C** (`kz_*`) — nenhum header ou dependência interna é exposta.

## Modelo

```
┌─────────────────────────────────────────────┐
│  Seu jogo (assembly C#)                     │
│  ┌───────────────────────────────────────┐  │
│  │  Kizuri.Scripting (API pública)       │  │
│  └───────────────────────────────────────┘  │
│         │  ABI C (kz_*)                     │
└─────────────────────────────────────────────┘
                 │
        ┌──────────────────┐
        │  Motor (C++20)   │  ← privado
        └──────────────────┘
```

## Ciclo de vida

1. **`[GameEntryPoint]`** — método que registra seus scripts em `GameModule`;
2. **`Script`** — classe que você herda, com callbacks de gameplay;
3. O editor/`KizuriGame` instancia os scripts registrados e chama os
   callbacks a cada frame.

## Registro de scripts

```csharp
public static class MeuGameModule
{
    [Kizuri.GameEntryPoint]
    public static void RegisterAll()
    {
        Kizuri.GameModule.Register<Jogador>("Jogador");
        Kizuri.GameModule.Register<Inimigo>("Inimigo");
    }
}
```

No editor: **Arquivo → Carregar GameModule…** aponta para a DLL. No **Play**,
o C# é **recompilado e recarregado automaticamente** — se a compilação falhar,
o Play é abortado e o erro aparece no Console.

## Onde colocar o código

Um projeto C# que referencia `Kizuri.Scripting`:

```bash
dotnet new classlib -o MeuJogo
dotnet add MeuJogo reference ../../managed/Kizuri.Scripting/Kizuri.Scripting.csproj
```

## Namespaces

- `Kizuri` — tudo: `Script`, `Entity`, `Scene`, `Input`, `Time`, `Audio`,
  `Mathf`, `SaveSystem`, `Rand`, `Log`, `GameModule`…
- `Kizuri.Math` — `Vector2`, `Vector3`, `Vector4`.

## Callbacks principais

```csharp
public override void OnCreate() { }                          // nasceu
public override void OnUpdate(float deltaSeconds) { }        // a cada frame
public override void OnCollisionBegin(Entity other) { }      // colidiu
public override void OnCollisionEnd(Entity other) { }        // saiu da colisão
public override void OnDestroy() { }                         // morreu
```

## Próximos passos

- [Classe Script e corrotinas](script.html) — callbacks em detalhe;
- [Entity — referência](entity.html) — fazer coisas com entidades;
- [Scene — referência](scene.html) — criar entidades, prefabs, queries;
- [Exemplos completos](exemplos.html) — código de jogo real.
