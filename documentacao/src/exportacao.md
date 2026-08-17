---
title: Exportar o jogo
group: Distribuição
order: 1
---

# Exportar o jogo

O menu **Arquivo → Exportar Jogo...** exporta cada plataforma do jeito
certo, sem misturar:

| Plataforma | O que sai | Como |
|---|---|---|
| **Windows** | Pasta pronta com `KizuriGame.exe` | **Exportar** (local, no PC Windows) |
| **Linux** | Pasta pronta (binários) + `.AppImage` | **Exportar** (local, no PC Linux); o AppImage também sai na CI |
| **Android** | `SeuJogo-android-arm64.apk` | **Exportar (Android)** — a engine compila tudo no seu PC se as ferramentas existirem |

## Como exportar

1. Tenha uma cena salva e defina-a como **cena inicial** (Arquivo → Definir
   cena como inicial). Os scripts ficam em `Source/` do projeto.
2. **Arquivo → Exportar Jogo...** e escolha a plataforma no seletor.
3. Preencha **build settings** (nome, versão, resolução — ficam no projeto).
4. Escolha a pasta de destino e clique em **Exportar**.

## Windows / Linux (local)

Sai uma pasta pronta pra distribuir:

```
MinhaPasta/
  KizuriGame.exe (ou binário Linux)   ← o jogo
  KizuriEngine.dll / .so              ← a engine
  cena.kzscene                        ← cena inicial
  assets/...                          ← modelos, texturas, áudios
  Kizuri.Scripting.dll                ← seu código C# compilado
```

## Android (.apk)

A engine compila o APK **no seu PC** se as ferramentas estiverem
instaladas (Android Studio SDK + NDK + .NET 10):

- SDK/NDK: `ANDROID_HOME` e `ANDROID_NDK_HOME` (ou instalados via
  `sdkmanager`); build-tools com `aapt2`/`zipalign`/`apksigner` e JDK.
- A janela de export mostra **o que falta** se você não tiver algo.
- O pipeline é: `cmake` (NDK) → `dotnet publish -r android-arm64`
  (CoreCLR) → `aapt2` → zip de libs/assets → alinhamento → assinatura.

::: dica
Sem SDK na máquina, o mesmo `.apk` é gerado pela **CI do GitHub** ao
publicar uma tag `vX.Y.Z` (job `android`).
:::

## AppImage (Linux)

Um único arquivo executável: **KizuriGame-linux-x86_64.AppImage** (gerado
pela CI no push de tag, ou pelo script `platform/linux/make-appimage.sh`).
Roda sem instalar nada.

## Atualização automática (updater)

A engine (editor) consulta a API do seu site:

```
GET https://seusite.com/api/version
→ { "version": "0.37.0", "download_url": "https://.../MeuJogo-windows.zip" }
```

- Se houver versão maior → pergunta **"Nova versão disponível — deseja
  atualizar?"** (`Sim` / `Não` + "não perguntar novamente").
- O `download_url` precisa apontar pra um **ZIP de verdade** (não TAR) com
  `bin/KizuriEditor` + `bin/KizuriEngine` + `bin/managed/` — o formato dos
  zips da Release. A URL fica em **Ajuda → Configurar Atualizações...**.

## Releases (GitHub)

`git tag vX.Y.Z && git push origin vX.Y.Z` → a CI publica os pacotes de
todas as plataformas: zips Windows/Linux (no formato do updater), AppImage,
APK e o Content Pack.

::: dica
Teste o export cedo: defina a cena inicial, exporte e rode o executável.
Só a cena inicial é carregada automaticamente.
:::
