---
title: Áudio
group: Componentes
order: 9
---

# Áudio

O **AudioSource** toca sons na cena. Áudio 3D (posicional) é suportado para
sons de mundo; sons de UI/menu usam one-shot simples.

## Componente AudioSource

- **Clip** — arquivo de áudio (`.wav`, `.ogg`, `.mp3`)
- **Volume**
- **Espacializado** — o som diminui com a distância da câmera (3D)

## Do script

```csharp
// tocar/parar um AudioSource na entidade
Entity.PlayAudio();
Entity.StopAudio();

// um som avulso (sem entidade)
Audio.PlayOneShot("Assets/sons/coin.wav");
Audio.PlayOneShotAt("Assets/sons/explosao.wav", 0.6f, x, y, z);

// parar tudo (troca de cena, por exemplo)
Audio.StopAll();
```

## Dicas

- **PlayOneShotAt** usa um pool seguro de sons posicionais — pode chamar
  quantas vezes quiser.
- Sons se posicionam em relação à **câmera principal**.
- Para música, use um AudioSource com loop.

::: nota
A engine usa **miniaudio** — carrega formatos comuns sem dependência externa.
:::
