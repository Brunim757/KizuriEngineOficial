---
title: SaveSystem
group: Scripting C#
order: 10
---

# SaveSystem

Persistência simples de jogo em um arquivo **JSON** (`save.json` no diretório
de trabalho por padrão).

## Guardar valores

```csharp
SaveSystem.Set("score", 100);                      // int
SaveSystem.Set("nome", "Kizuri");                  // string
SaveSystem.Set("volume", 0.7f);                    // float
SaveSystem.Set("som", true);                       // bool
SaveSystem.Set("pos", new Vector3(1f, 2f, 3f));    // vetores
SaveSystem.Set("ponto", new Vector2(5f, 0f));
```

## Gravar em disco

```csharp
SaveSystem.Save();   // escreve save.json
```

Chame ao passar de fase, no pause, ou ao fechar.

## Ler (auto-carrega)

Os `Get*` **carregam do disco automaticamente** na primeira consulta:

```csharp
var score  = SaveSystem.GetFloat("score", 0f);        // fallback se não existir
var nome   = SaveSystem.GetString("nome", "sem nome");
var volume = SaveSystem.GetFloat("volume", 1f);
var som    = SaveSystem.GetBool("som", true);

if (SaveSystem.Has("pos"))
{
    var pos = SaveSystem.GetVector3("pos");
    jogador.SetPosition(pos);
}
var ponto = SaveSystem.GetVector2("ponto");
```

## Outros arquivos

```csharp
SaveSystem.SetPath("jogos/fase1.save");   // muda o arquivo (limpa o cache)
```

## Referência

| Método | Descrição |
|--------|-----------|
| `Set(key, string/int/float/bool/Vector2/Vector3)` | Guarda um valor |
| `Has(key)` | Existe a chave? |
| `GetString(key, fallback)` | Lê string |
| `GetInt(key, fallback)` | Lê int (aceita float salvo) |
| `GetFloat(key, fallback)` | Lê float (aceita int salvo) |
| `GetBool(key, fallback)` | Lê bool |
| `GetVector2(key, fallback)` / `GetVector3(key, fallback)` | Lê vetor |
| `Save()` | Grava em disco |
| `Load()` | Recarrega do disco |
| `SetPath(path)` | Muda o arquivo de save |
| `FilePath` | Caminho atual |

::: warn
`Save()` grava o estado **atual** do cache. Depois de `Set`, lembre de chamar
`Save()` — os valores não são persistidos sozinhos.
:::

## Exemplo de jogo

```csharp
// OnDestroy do jogador: salva a posição
public override void OnDestroy()
{
    if (Entity.TryGetWorldPosition(out var p))
    {
        SaveSystem.Set("pos", p);
        SaveSystem.Set("vidas", vidas);
        SaveSystem.Save();
    }
}

// Na abertura de uma fase, carregar a posição salva
public override void OnCreate()
{
    if (SaveSystem.Has("pos"))
        Entity.SetPosition(SaveSystem.GetVector3("pos"));
}
```
