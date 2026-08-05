---
title: AudioSource
group: Componentes
order: 9
---

# AudioSource

Emissor de som ligado a uma entidade — usa o **AudioEngine** (miniaudio por
baixo).

## AudioSourceComponent

| Campo | Descrição |
|-------|-----------|
| **ClipPath** | Arquivo de áudio (wav, mp3, ogg, flac…) |
| **Loop** | Repete ao fim |
| **PlayOnStart** | Toca ao entrar no Play |
| **Spatial** | `true` = atenua por distância + panorâmica 3D; `false` = volume fixo (música/UI) |
| **Volume** | Volume do source (0..1) |
| **MinDistance / MaxDistance** | Faixa de atenuação espacial |

## Como usar

1. Adicione o componente à entidade;
2. Escolha o `ClipPath` (relativo ao projeto);
3. No Play, o som toca (se `PlayOnStart`) e é **posicional** — mais perto da
   câmera = mais alto.

## Em C#

```csharp
Entity.AddAudio("Assets/Sounds/passo.wav", loop: true, playOnStart: true);
Entity.PlayAudio();
Entity.StopAudio();
```

## Sons avulsos (sem entidade)

Para SFX desamarrados de uma posição:

```csharp
Audio.PlayOneShot("Assets/Sounds/tiro.wav");               // avulso
Audio.PlayOneShot("Assets/Sounds/tiro.wav", volume: 0.8f);

// POSICIONAL 3D — toca no ponto do mundo, atenua pela distância
// ao ouvinte (câmera). Perfeito para impactos, passos, tiros.
Audio.PlayOneShotAt("Assets/Sounds/impacto.wav", 1f, position);

Audio.StopAll();                     // para tudo (troca de cena)
Audio.SetMasterVolume(0.5f);         // volume mestre global
```

::: warn
**Só toca no Play.** Fontes de áudio (como física e partículas) só são
processadas durante o **Play** / jogo exportado.
:::

## O ouvinte

O ouvinte é a **câmera primária** da cena. Movendo a câmera, o som espacial
acompanha.
