---
title: Recursos embutidos (kzres://)
group: Distribuição
order: 2
---

# Recursos embutidos (kzres://)

A engine é **self-contained**: os assets que ela usa como padrão ficam
**dentro do executável**, acessíveis pelo prefixo `kzres://`.

## Por que existe

Se o usuário apagar um arquivo ou rodar sem a pasta de conteúdo, **nenhuma
função padrão quebra**. O céu, as fontes e os modelos de demonstração vêm
embutidos.

## Como usar

```csharp
// qualquer slot de asset aceita kzres://
Entity.AddMeshRenderer("kzres://models/Fox.glb");
Entity.SetMaterialAlbedoMap("kzres://textures/...");
```

## Prioridade de carregamento

1. **Embutido** (`kzres://`) — sempre disponível
2. **Content Pack** (`content/`) — se o arquivo existir ao lado do jogo
3. **Builtins** — primitivas (`builtin:cube` etc.)

Ou seja: o Content Pack **enriquece** se estiver presente, mas nunca é
obrigatório.

## O que já vem embutido

- Modelos de demonstração (Fox animado, DamagedHelmet PBR, cubo)
- Céu `kzres://skies/sky_gradient.hdr`
- Fontes da UI (JetBrains Mono)

::: dica
Recursos embutidos são uma **garantia de funcionamento**. Para assets do
SEU jogo, use os arquivos normais do projeto — o export inclui tudo.
:::
