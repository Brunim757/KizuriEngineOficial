---
title: Seu primeiro projeto
group: Introdução
order: 3
---

# Seu primeiro projeto

Em ~10 minutos você terá um jogo 3D rodando com física e um script C#.

## 1. Crie o projeto

No Hub do editor, escolha **Novo Projeto**, dê um nome (ex.: `MeuJogo`) e
selecione o modo **3D**. O editor monta a estrutura de pastas e uma cena vazia.

## 2. Adicione um chão e um cubo

1. No painel **Hierarquia**, abra o menu **+** e adicione **Cubo 3D**.
2. No **Inspetor**, em Transform, posicione o cubo em `Y = 0.5`.
3. Adicione um **Plano 3D** (serve de chão) na posição `(0, 0, 0)`.
4. Adicione uma **Luz Direcional** (o sol) e uma **Câmera 3D** (posicione em `(0, 2, 5)` olhando para o cubo).

::: dica
O viewport 3D navega com **botão direito + WASD**. Selecione a câmera e use
os **gizmos** (teclas W/E/R) para posicionar.
:::

## 3. Dê física ao cubo

No **Inspetor** do cubo, clique em **Adicionar Componente → Rigidbody 3D** e
depois **Colisor Box 3D**. Agora aperte **Play** (F5): o cubo cai no chão e
para. Aperte **Stop** (Shift+F5).

## 4. Escreva um script

1. No **Content Browser**, navegue até `Source/` (use o seletor de pasta
   na barra do painel: **Raiz / Conteúdo (assets) / Source**), clique com o
   botão direito → **Criar Script C#...** e renomeie o arquivo pra `Girar.cs`
   (botão direito no item → Renomear).
2. Abra o arquivo e escreva:

```csharp
using Kizuri;

public sealed class Girar : Script
{
    public override void OnUpdate(float deltaSeconds)
    {
        // Rotaciona a entidade 90 graus por segundo
        var e = Entity.Rotation; // euler em radianos
        Entity.SetRotation(new Math.Vector3(e.X, e.Y + 90f * deltaSeconds, e.Z));
    }
}
```

3. Selecione o cubo e, no Inspetor, **+ Adicionar Componente → Script C# →
   Girar** (o componente registra a classe sozinho no Play — nem precisa do
   registro manual).
4. Aperte **Play**: o cubo cai e gira.

::: nota
Os scripts live em `Source/` do projeto (o Content Browser te leva pra lá).
Qualquer classe pública `: Script` vira opção no dropdown automaticamente.
:::

## 5. Exporte o jogo

Menu **Arquivo → Exportar Jogo...**: no seu PC gera a pasta com o
`KizuriGame` (Windows/Linux) ou o **APK Android** (veja
[Exportar](exportacao.html)).

Continue na seção [Editor](interface.html) para conhecer cada painel, ou
[Transform](transform.html) para os componentes de cena.
