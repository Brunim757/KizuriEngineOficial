---
title: Recursos embutidos (kzres://)
group: Conceitos
order: 3
---

# Recursos embutidos (kzres://)

## O problema que resolve

Se uma função padrão da engine dependesse de um arquivo no disco, apagar esse
arquivo (ou esquecer de copiá-lo) **quebraria** a função. A Kizuri resolve
isso compilando os assets que usa como padrão **dentro do executável**.

## O esquema `kzres://`

Um caminho `kzres://<nome>` referencia um recurso embutido no binário. Ele
funciona em **qualquer campo de asset** (mesh, textura, HDRI, skin):

| Recurso | Uso |
|---------|-----|
| `kzres://models/Cube.glb` | Cubo (demonstração 3D) |
| `kzres://models/Fox.glb` | Fox animado (skinning) — demo 3D |
| `kzres://models/DamagedHelmet.glb` | Capacete PBR — demo 3D |
| `kzres://skies/sky_gradient.hdr` | Céu HDRI padrão |

## Prioridade ao carregar

Quando um asset de demonstração é usado, a ordem é sempre:

```
1. Embutido (kzres://)     → sempre disponível
2. Content Pack no disco   → mais rico, se presente
3. Builtins procedurais    → malhas geradas em código
```

::: ok
**Consequência prática:** apague a pasta `content/` do lado do executável e a
demonstração 3D continua funcionando — o Fox, o capacete e o céu já estão
dentro do binário.
:::

## O Content Pack

Os pacotes distribuídos acompanham um **Content Pack** (modelos glTF, HDRIs de
céu e texturas PBR, todos CC0). Ele **enriquece** as demos — mas **nunca é
requisito** para o motor funcionar.

## Na prática

```csharp
// carrega um modelo embutido como qualquer outro asset
Entity f = Scene.CreateEntity("Fox");
f.AddMeshRenderer("kzres://models/Fox.glb");
```

No editor, basta digitar o caminho `kzres://…` no campo de mesh — ou usar a
**demonstração 3D** (menu Cena), que já usa os embutidos automaticamente.
