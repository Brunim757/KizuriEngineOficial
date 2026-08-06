---
title: SaveSystem
group: Scripting C#
order: 7
---

# SaveSystem

Salve e carregue o progresso do jogador em **JSON**, sem depender da engine —
é 100% gerenciado (puro C#).

## Salvar

```csharp
public class Progresso
{
    public int Fase = 1;
    public int Moedas = 0;
    public string Nome = "";
}

// salvar num arquivo (o caminho é relativo ao jogo)
SaveSystem.Save("save.json", new Progresso { Fase = 3, Moedas = 42 });
```

## Carregar

```csharp
var p = SaveSystem.Load<Progresso>("save.json");
if (p != null) Debug.Log($"Fase {p.Fase}, moedas {p.Moedas}");
```

## O que funciona

- Objetos simples com campos públicos (serializados automaticamente)
- Listas e dicionários
- Tipos básicos (int, float, string, bool)

::: dica
Use `SaveSystem` para **checkpoints** e **configurações** do jogador. Para
salvar a cena inteira (nível montado no editor), use arquivos `.kzscene`.
:::
