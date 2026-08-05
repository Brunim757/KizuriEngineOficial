---
title: Instalação e build
group: Introdução
order: 2
---

# Instalação e build

## Requisitos

| Item | Requisito |
|------|-----------|
| **CMake** | ≥ 3.24 |
| **Compilador** | C++20 (MSVC / MinGW / GCC / Clang) |
| **OpenGL** | mínimo **3.3 core** (4.0+ libera o modo "ultra") |
| **Build** | Ninja (opcional, usado nos presets) |
| **.NET SDK** | 8+ (para compilar o assembly C# do jogo) |

::: info
**Dependências automáticas.** GLFW, glm, EnTT, spdlog, Bullet3, Box2D,
miniaudio, stb, tinyobjloader, Dear ImGui, ImGuizmo e nlohmann/json são
baixadas por `FetchContent`. O `glad` (GL 4.5 core) é pré-gerado e comitado.
Você não instala nada manualmente.
:::

## Compilando

```bash
# configurar + compilar (Release)
cmake --preset linux-release      # ou windows-release / debug
cmake --build --preset linux-release
```

Os binários vão para `build/bin/` (ou `build-debug/bin/`):

| Executável | O que é |
|------------|---------|
| `KizuriEditor` | O editor visual completo |
| `KizuriGame` | Executável de jogo standalone (cena inicial + assembly C#) |
| `Sandbox` | Exemplo mínimo de uso da engine via API |

## Opções do CMake

| Opção | Padrão | Descrição |
|-------|--------|-----------|
| `KZ_BUILD_EDITOR` | `ON` | Compila o editor |
| `KZ_BUILD_GAME` | `ON` | Compila o jogo standalone |
| `KZ_BUILD_SANDBOX` | `ON` | Compila o exemplo sandbox |
| `KZ_BUILD_TESTS` | `OFF` | Testes unitários (não há suíte ainda) |
| `KZ_EMBED_DEMO_DIR` | — | Pasta com assets de demonstração a **embutir** no binário (`kzres://`) |

::: ok
**Assets de demonstração embutidos.** Na distribuição oficial, Fox animado e
capacete PBR são baixados no build e **compilados dentro do executável**.
Sem isso, a demo 3D cai para os builtins — nenhuma função padrão quebra.
:::

## Primeira execução

Abra o `KizuriEditor`. A primeira tela é o **hub** de projetos:
- **Novo Projeto** — escolha o modo **2D**, **3D** ou **Vazio**;
- **Abrir Projeto** — carregue um `.kzproj` existente;
- **Voltar ao Início** — retorna ao hub (menu Arquivo).

Aperte **F5** para testar a cena com física e scripts reais.
