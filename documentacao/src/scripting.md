---
title: Introdução ao C#
group: Scripting C#
order: 1
---

# Scripting C#

A lógica do seu jogo é escrita em **C#**, com uma API simples e completa
chamada `Kizuri.Scripting`. Você cria classes que respondem a eventos do
jogo, e o editor cuida do resto.

## Como funciona

1. Você escreve uma classe que herda de `Script`;
2. Você registra essa classe em um **GameModule**;
3. O editor (ou o jogo exportado) cria as instâncias e chama seus métodos
   quando o jogo pede: quando a entidade nasce, a cada frame, quando colide…

## Um script mínimo

```csharp
using Kizuri;

public sealed class MeuJogador : Script
{
    public override void OnCreate() { }                        // quando nasceu
    public override void OnUpdate(float deltaSeconds) { }      // a cada frame
    public override void OnCollisionBegin(Entity other) { }    // colidiu
    public override void OnCollisionEnd(Entity other) { }      // saiu da colisão
    public override void OnDestroy() { }                       // quando morreu
}
```

## Registrando seus scripts

```csharp
public static class MeuGameModule
{
    [Kizuri.GameEntryPoint]
    public static void RegisterAll()
    {
        Kizuri.GameModule.Register<MeuJogador>("MeuJogador");
        Kizuri.GameModule.Register<Inimigo>("Inimigo");
    }
}
```

## Usando no editor

1. Compile seu projeto C# — a **DLL** gerada é o que o editor carrega;
2. **Arquivo → Carregar GameModule…** e escolha a DLL;
3. No **Play**, o editor **recompila e recarrega seu código automaticamente**
   — alterou o C#, salvou, apertou Play de novo, já rodou a versão nova. Se
   houver erro de compilação, o Play é abortado e o erro aparece no Console.

## O que a API oferece

| Área | Classe/namespace |
|------|------------------|
| Entidades e componentes | `Entity` |
| Cena, prefabs e queries | `Scene` |
| Teclado e mouse | `Input` |
| Tempo | `Time` |
| Matemática | `Mathf` + `Kizuri.Math` (Vector2/3) |
| Áudio | `Audio` |
| Salvamento | `SaveSystem` |
| Aleatório e log | `Rand`, `Log` |

## Próximos passos

- [Classe Script e corrotinas](script.html) — callbacks e ações com espera;
- [Entity — referência](entity.html) — o que dá para fazer com uma entidade;
- [Scene — referência](scene.html) — criar entidades, prefabs, detecção;
- [Exemplos completos](exemplos.html) — código de jogo pronto.
