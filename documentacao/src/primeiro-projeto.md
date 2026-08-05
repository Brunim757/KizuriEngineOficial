---
title: Seu primeiro projeto
group: Introdução
order: 3
---

# Seu primeiro projeto

Este guia leva do zero a um jogo rodando com física e script C#, em ~10
minutos.

## 1. Crie o projeto

1. Abra o **KizuriEditor**.
2. **Novo Projeto**, dê um nome e escolha o modo:
   - **2D** — cena já nasce com câmera ortográfica, chão com física e uma
     caixa que cai;
   - **3D** — câmera de perspectiva, cubo de exemplo e sol direcional;
   - **Vazio** — só uma câmera.

## 2. Teste com Play

Aperte **F5** (ou o botão ▶ da toolbar). A caixa do projeto 2D **cai de
verdade** (Box2D). **Shift+F5** para voltar à edição.

::: info
O **Play** roda uma cópia isolada da cena — a edição nunca é tocada. Física,
scripts, partículas e áudio só funcionam durante o Play.
:::

## 3. Adicione coisas ao viewport

- Selecione a entidade **Chão** na Hierarquia.
- Use o **Inspetor** para mudar cor, tamanho e física.
- Crie entidades: botão direito na Hierarquia (ou menu Editar).
- Adicione componentes pelos botões **Adicionar** do Inspetor.

## 4. Escreva um script C#

Crie um projeto C# que referencia `Kizuri.Scripting`:

```csharp
using Kizuri;
using Kizuri.Math;

public sealed class MeuJogador : Script
{
    public override void OnCreate() { }

    public override void OnUpdate(float deltaSeconds)
    {
        if (Input.IsKeyPressed(Key.A)) Entity.SetPosition(new Vector3(-1f, 0f, 0f));
        if (Input.IsKeyPressed(Key.D)) Entity.SetPosition(new Vector3( 1f, 0f, 0f));
    }

    public override void OnCollisionBegin(Entity other) { }
    public override void OnCollisionEnd(Entity other) { }
    public override void OnDestroy() { }
}

public static class MeuGameModule
{
    [Kizuri.GameEntryPoint]
    public static void RegisterAll()
    {
        Kizuri.GameModule.Register<MeuJogador>("MeuJogador");
    }
}
```

## 5. Carregue o GameModule

1. Compile a DLL (ex.: `dotnet build`).
2. No editor: **Arquivo → Carregar GameModule…** e escolha a DLL.
3. Selecione a entidade do jogador no Inspetor e adicione o componente de
   script **MeuJogador**.
4. Aperte **Play** — o editor **recompila e recarrega o C# automaticamente**
   (se compilar com erro, o Play é abortado e o erro aparece no Console).

## 6. Exporte

**Arquivo → Exportar Jogo…** gera um executável standalone com a cena inicial
e o assembly. Veja [Exportar o jogo](exportacao.html).

## Próximos passos

- Conheça os [painéis do editor](interface.html).
- Aprenda cada [componente](transform.html).
- A referência completa da [API C#](scripting.html).
