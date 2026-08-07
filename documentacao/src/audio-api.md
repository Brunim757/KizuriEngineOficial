---
title: Áudio (API C#)
group: Scripting C#
order: 9
---

# Áudio — API C#

Controle de som e música.

## Na entidade

```csharp
Entity.AddAudio("Assets/sons/pulo.wav");
Entity.PlayAudio();
Entity.StopAudio();
```

## One-shot

```csharp
Audio.PlayOneShot("Assets/sons/moeda.wav");
Audio.PlayOneShotAt("Assets/sons/explosao.wav", 0.7f, x, y, z);
```

## Audio mixer (grupos)

Volumes independentes por tipo — o AudioSource tem o campo **Grupo**
(SFX, Música ou UI) e o one-shot entra no grupo SFX por padrão:

```csharp
Audio.MusicVolume = 0.7f;  // abaixa a música
Audio.SFXVolume  = 1.0f;   // efeitos cheios
Audio.UIVolume   = 0.9f;   // sons de menu
```

## Global

```csharp
Audio.StopAll();
```

## Dicas

- **PlayOneShotAt** é posicional (volume cai com a distância da câmera) e
  usa um pool seguro — chame sem medo em alta frequência.
- Música de fundo: um `AudioSource` com loop na cena.
- Sons de UI/menu: `PlayOneShot` simples.

Veja também [Áudio (componente)](audio-source.html).
