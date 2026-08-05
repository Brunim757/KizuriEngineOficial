---
title: Classe Script e corrotinas
group: Scripting C#
order: 2
---

# Classe Script

A base de todo comportamento de jogo. Você herda de `Script` e sobrescreve os
callbacks.

## Callbacks

| Método | Quando roda |
|--------|-------------|
| `OnCreate()` | Ao instanciar o script (entrada no Play / instanciar prefab) |
| `OnUpdate(float dt)` | Todo frame, com o delta em segundos |
| `OnCollisionBegin(Entity other)` | Começou a colidir com outra entidade |
| `OnCollisionEnd(Entity other)` | Deixou de colidir |
| `OnDestroy()` | Ao destruir a entidade / sair do Play |

## Propriedades

| Propriedade | Tipo | Descrição |
|-------------|------|-----------|
| `Entity` | `Entity` | A entidade dona deste script |

## Estrutura mínima

```csharp
using Kizuri;

public sealed class MeuScript : Script
{
    public override void OnCreate() { }
    public override void OnUpdate(float deltaSeconds) { }
    public override void OnCollisionBegin(Entity other) { }
    public override void OnCollisionEnd(Entity other) { }
    public override void OnDestroy() { }
}
```

## Corrotinas

Ações diferidas no tempo, no estilo Unity, usando `IEnumerator`:

```csharp
using System.Collections;
using Kizuri;

public sealed class Temporizador : Script
{
    public override void OnCreate()
    {
        StartCoroutine(Rotina());
    }

    IEnumerator Rotina()
    {
        Log.Info("Vai esperar 1 segundo…");
        yield return new WaitForSeconds(1f);
        Log.Info("1 segundo depois!");

        yield return new WaitForFrames(60);
        Log.Info("mais 60 frames…");
    }
}
```

### Tipos de espera

| Tipo | Espera |
|------|--------|
| `new WaitForSeconds(s)` | `s` segundos de tempo de jogo |
| `new WaitForFrames(n)` | `n` frames |

::: info
Corrotinas são atualizadas **depois de cada `OnUpdate`** — você não gerencia
nada. Remova a corrotina destruindo o script/entidade.
:::

## Estado entre frames

Guarde estado em campos da classe — o script vive enquanto a entidade existe:

```csharp
public sealed class Coletor : Script
{
    private int _moedas;

    public override void OnCollisionBegin(Entity other)
    {
        if (other.Name == "Moeda")
        {
            _moedas++;
            Log.Info($"Moedas: {_moedas}");
            other.Destroy();
        }
    }
}
```

## Scripts e o editor

No Inspetor, o botão de adicionar script lista os scripts **registrados no
GameModule carregado**. Adicione o componente, escolha o script, e ele roda no
Play.
