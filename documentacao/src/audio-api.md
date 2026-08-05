---
title: Audio (API)
group: Scripting C#
order: 9
---

# Audio — API global

Áudio **avulso** (sem entidade) e controles globais. Áudio ligado a entidade
(posicional) usa `Entity.AddAudio` / `Entity.PlayAudio`.

## One-shot avulso

```csharp
Audio.PlayOneShot("Assets/Sounds/tiro.wav");
Audio.PlayOneShot("Assets/Sounds/tiro.wav", volume: 0.8f);
```

## One-shot posicional (3D)

```csharp
// toca no ponto do mundo, atenuado pela distância ao ouvinte (câmera)
Audio.PlayOneShotAt("Assets/Sounds/impacto.wav", 1f, new Vector3(0f, 1f, 0f));
```

Perfeito para **impactos, passos, tiros** — som que "acontece no lugar".

```csharp
// impacto onde um tiro acertou
if (Scene.Raycast3D(from, to, out var hit, out var point, out _))
    Audio.PlayOneShotAt("Assets/Sounds/impacto.wav", 1f, point);
```

## Controles globais

| Método | Descrição |
|--------|-----------|
| `Audio.StopAll()` | Para todos os sons (usado em troca de cena) |
| `Audio.SetMasterVolume(float)` | Volume mestre global (0..1) |

## Áudio de entidade

```csharp
var som = Scene.CreateEntity("Motor");
som.AddAudio("Assets/Sounds/motor.wav", loop: true, playOnStart: true);
som.PlayAudio();
som.StopAudio();
```

Ver [AudioSource](audio-source.html) para as propriedades (spatial, volume,
distâncias).
