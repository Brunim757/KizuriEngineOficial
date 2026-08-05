---
title: Hierarquia e transform
group: Conceitos
order: 2
---

# Hierarquia e transform

## Entidades

Uma **entidade** é um recipiente de **componentes** — ela só existe porque
carrega componentes. As entidades vivem em uma **árvore** (hierarquia): uma
entidade pode ter um **pai** e vários **filhos**.

```
Cena
└─ Jogador
   ├─ Corpo
   └─ Arma
      └─ PontoDeTiro
```

O painel **Hierarquia** mostra essa árvore. Arraste uma entidade sobre outra
para parentar; arraste para fora para destacar.

## Transform

Toda entidade tem um **TransformComponent** — posição, rotação e escala:

| Campo | Tipo | Descrição |
|-------|------|-----------|
| **Translation** | `Vector3` | Posição local em unidades do mundo |
| **Rotation** | `Vector3` (euler) | Rotação em **radianos** |
| **Scale** | `Vector3` | Escala local (1 = 100%) |

A rotação no editor é exibida em graus; no C#, é em **radianos**.

## Local vs. mundial

Filhos herdam o transform do pai. Isso significa:

- Mova o **pai** → os **filhos vão junto**;
- A posição de um filho é **relativa** ao pai (local);
- A **posição mundial** de um filho considera a cadeia inteira.

Em C#:

```csharp
var pos = new Vector3(1f, 2f, 0f);
Entity.SetPosition(pos);            // posição LOCAL
Entity.SetRotation(new Vector3(0f, 0f, 0.5f)); // euler em radianos
Entity.SetScale(new Vector3(2f, 2f, 2f));

filho.SetParent(pai);               // parenta em runtime
filho.SetParent();                  // destaca

if (filho.TryGetWorldPosition(out var world)) { } // posição MUNDIAL
filho.LookAt(new Vector3(0f, 0f, 0f));            // encara um ponto
```

## Gizmo no editor

Com a entidade selecionada, use os gizmos do viewport:

- <kbd>W</kbd> — **mover**
- <kbd>E</kbd> — **rotacionar**
- <kbd>R</kbd> — **escalar**

O gizmo trabalha em **espaço local** por padrão e respeita a hierarquia ao
mover pais e filhos juntos.

::: warn
**Ordem TRS.** O transform é aplicado como T · R · S (translate → rotate →
scale). Escalar um pai distorce os filhos na proporção da escala dele.
:::
