---
title: Rand e Log
group: Scripting C#
order: 11
---

# Rand e Log

Utilidades de aleatoriedade e registro de mensagens.

## Rand

Números aleatórios para o jogo. O nome é curto de propósito — "Random"
colidiria com `System.Random`.

| Método | Descrição |
|--------|-----------|
| `Rand.Float(min, max)` | Float em **[min, max]** |
| `Rand.Int(min, max)` | Int em **[min, max)** (min incluso, max excluso) |
| `Rand.Chance(probability)` | `true` com probabilidade (0..1) |
| `Rand.Vector2(minX, maxX, minY, maxY)` | Vetor 2D aleatório |
| `Rand.Vector3(minX, maxX, minY, maxY, minZ, maxZ)` | Vetor 3D aleatório |

```csharp
var dano = Rand.Int(5, 11);            // 5,6,7,8,9,10
var pos  = Rand.Vector3(-20, 20, 0, 10, -20, 20);
if (Rand.Chance(0.3f)) DropItem();     // 30% de chance

// campo de destroços
var destroco = Scene.CreateEntity("Destroço");
destroco.AddSprite();
destroco.SetPosition(new Vector3(
    Rand.Float(-5f, 5f), Rand.Float(0f, 5f), Rand.Float(-5f, 5f)));
```

## Log

Registro de mensagens no **Console** do editor.

| Método | Nível |
|--------|-------|
| `Log.Info(msg)` | Informação |
| `Log.Warn(msg)` | Aviso (amarelo) |
| `Log.Error(msg)` | Erro (vermelho) |

```csharp
Log.Info($"Jogador {Entity.Id} pronto.");
Log.Warn($"Vida baixa: {vida}");
Log.Error("Falha ao carregar asset!");
```

::: info
Use `Log` para depurar — as mensagens aparecem no painel **Console** com
filtro por nível.
:::
