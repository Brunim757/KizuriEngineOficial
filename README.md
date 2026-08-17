# Kizuri Engine

> **v0.37.1** — (foco: facilidade de scripts) engine de jogos 2D + 3D em C++20 com editor visual integrado.
> Projeto pessoal em desenvolvimento ativo: hoje a v1 já permite criar jogos
> híbridos (2D e 3D na mesma cena) e empacotá-los num executável standalone.

> Repositório privado. O `docs/NOTAS_INTERNAS.md` registra o estado real, sem
> filtro (o que funciona, o que é gambiarra e o que falta).

> **Self-contained:** os assets que a engine usa como padrão (modelos da demo
> 3D — Fox animado e DamagedHelmet PBR —, céu, primitivas) vêm **embutidos no
> executável** via `kzres://`. Nenhuma função padrão quebra se um arquivo for
> apagado; o Content Pack da distribuição só enriquece as demos, nunca é
> requisito.

> 📖 **Documentação (site):** o site completo, multi-página e com busca, fica em
> [`documentacao/`](documentacao/src/index.md) — fonte em Markdown
> (`generate_site.py` gera `documentacao/site/`, um site estático pronto para
> hospedar em qualquer lugar).

---

## ✨ Destaques

### Renderer 3D
- **PBR completo** (Cook-Torrance: GGX + Smith + Fresnel-Schlick) com metallic/roughness
- **Multi-luz dinâmica** — até 16 luzes por frame (Directional / Point / Spot)
- **IBL** — céu procedural → cubemap → irradiância difusa + pré-filtro especular GGX
- **Sombras em cascata (CSM)** em 3 faixas que acompanham a câmera
- **Bloom + tonemap HDR (ACES)** com pipeline de bright-pass + blur gaussiano
- **Normal mapping** via derivada de tela (sem tangentes na mesh)
- **Partículas GPU-instanced** com billboarding
- **Meshes** builtin (cubo, plano, esfera) + import de `.obj` com material serializável

### Renderer 2D
- Sprites com textura (path serializável) e tiling
- **Círculos** (SDF, disco ou anel com borda suavizada)
- **Texto / HUD** — `TextComponent` com fonte embutida (JetBrains Mono, atlas via stb_truetype)
- **Tilemaps** — grade de tiles com atlas de textura (colunas e linhas explícitas)
- **Animação de sprite** — sprite sheets por frames, com preview no viewport
- **Grid de referência** no plano XY com eixos destacados

### Jogo
- **ECS** (EnTT): Transform, SpriteRenderer, MeshRenderer, Camera, Light,
  ParticleSystem, AudioSource, Rigidbody 2D/3D + colliders, NativeScript
- **Física** — Box2D (2D) e Bullet3 (3D), simuladas em runtime
- **Áudio** — miniaudio, espacial 3D (atenuação por distância) ou fixo; `Play()/Stop()`
  por script/colisão/evento, além do `PlayOnStart`
- **Scripting** — **C#** via `Kizuri.Scripting` (única API pública, estilo
  Unity/Godot .NET). A engine C++ fica 100% privada: o assembly .NET conversa
  com ela apenas por um ABI C (`kz_*`), sem expor headers/deps internos

### API C# de gameplay
- **Entidades**: `Scene.CreateEntity`, `Scene.InstantiatePrefab`, `Scene.Load`,
  `Scene.GetPrimaryCamera`, `Entity.Destroy`; adicionar componentes em runtime
  (`AddSprite`, `AddText`, `AddAudio`, `AddCamera`)
- **Mutação em runtime**: sprite (textura/cor), texto (conteúdo/tamanho/cor),
  áudio (`PlayAudio`/`StopAudio`)
- **UI interativo**: `AddUICanvas` + `AddUIRect`/`AddUIButton`/`AddUIText`,
  com `UIButtonWasClicked()`/`UIButtonIsHovered()` — menus, HUDs e botões de
  verdade (espaço de tela, hover/pressed/clique)
- **Corrotinas** estilo Unity (`StartCoroutine` + `WaitForSeconds`), **`Time.TimeScale`**
  (câmera lenta), **`Rand`**, **`SaveSystem`** (JSON), input de mouse,
  `Scene.Raycast2D`, `Audio.PlayOneShot`
- Exemplo completo em `managed/SampleGame/PlayerController.cs` e
  `managed/SampleGame/UISample.cs`
- **Serialização** — cenas em `.kzscene` (JSON legível), incluindo mesh, material e texturas

### Editor (KizuriEditor)
- Dockspace ImGui + gizmos (mover/rotacionar/escalar)
- Hierarquia, Inspetor (painéis para todos os componentes), Console com filtro
- Content Browser com **drag & drop** de `.obj`/`.png` direto para o viewport
- **Play/Stop** com cópia isolada da cena (edição nunca toca a original) e
  **compilação automática do C# no Play** (estilo Unity: recompila e recarrega
  o assembly; se falhar, o Play é abortado com o erro)
- **Undo/Redo**, picking por clique no viewport (3D por raycast, 2D por ponto)
- Toolbar do viewport com ícones: Mover/Rotacionar/Escalar, 2D/3D e Play/Stop
  (atalhos W/E/R, F5=Play, Shift+F5=Stop); o Play usa a câmera da **própria
  cena**, não a câmera livre de navegação do editor
- Modos de viewport 2D / 3D, gizmo de câmera, diálogos nativos de arquivo (Windows;
  Linux/macOS usam campo de texto manual)
- Content Browser lista arquivos da pasta atual — abrir um projeto (`.kzproj`)
  ainda não expõe a pasta `Assets/` na UI

---

## 🚀 Build

Requisitos: **CMake ≥ 3.24**, compilador **C++20** (MSVC / MinGW / GCC / Clang),
**Ninja** (opcional, usado nos presets). Todas as dependências são baixadas
automaticamente via **FetchContent** (GLFW, glm, EnTT, spdlog, Bullet3, box2d,
miniaudio, stb, tinyobjloader, Dear ImGui, ImGuizmo, nlohmann/json). O `glad`
(GL 4.5 core) é pré-gerado e comitado em `third_party_generated/glad/`.

```bash
# Linux / Windows (configurar + compilar)
cmake --preset linux-release      # ou windows-release / debug
cmake --build --preset linux-release
```

Binários vão para `build/bin/` (ou `build-debug/bin/`):

| Executável   | O que é |
|--------------|---------|
| `KizuriEditor` | O editor visual |
| `KizuriGame`   | Jogo standalone — roda a cena inicial + GameModule |
| `Sandbox`      | Exemplo mínimo de uso da API pela engine |

### Android (APK)

O MESMO runtime C# (CoreCLR, não Mono) roda no Android: a CI
(`.github/workflows/build.yml`, job `android`) compila `libKizuriGame.so`
(engine + EGL/GLES 3.x — sem GLFW/ImGui, que são desktop) e embute o
runtime .NET publicado com `dotnet publish -r android-arm64 --self-contained`
— `libhostfxr.so`/`libcoreclr.so` e os assemblies do jogo vão nos assets do
APK e são extraídos pro `filesDir` na primeira execução (`AndroidEntry.cpp`).
Instale via `adb install KizuriGame-android.apk` (`adb logcat -s Kizuri`).

```bash
# Manual (NDK r27 + build-tools 34 + .NET 10 no PATH):
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/27.2.12479018
cmake --preset android-arm64
cmake --build --preset android-arm64 --target KizuriGame
dotnet publish managed/SampleGame -c Release -r android-arm64 --self-contained -o android_dotnet
platform/android/package_apk.sh build-android/bin android_dotnet content/game \
  "$ANDROID_HOME/build-tools/34.0.0" KizuriGame-android.apk
```

Limitações atuais do mobile: sem ImGui/editor, sem teclado físico (input por
toque = mouse + teclas virtuais via `SetActionKey`), `SaveToFile`/filedialog
desktop-only, e iOS ainda não suportado (JIT proibido — precisaria de
NativeAOT).

Opções do CMake:

| Opção              | Padrão | Descrição |
|--------------------|--------|-----------|
| `KZ_BUILD_EDITOR`  | `ON`   | Compila o editor |
| `KZ_BUILD_GAME`    | `ON`   | Compila o executável de jogo standalone |
| `KZ_BUILD_SANDBOX` | `ON`   | Compila o exemplo sandbox |
| `KZ_BUILD_TESTS`   | `OFF`  | Testes unitários (ainda não há suíte) |

---

## 🎮 Como usar

**Editor** — monte a cena visualmente: adicione entidades, componentes,
arraste assets do Content Browser para o viewport e aperte **Play** para testar.

**Jogo standalone** — exporte sua cena como `.kzscene` e rode:

```bash
KizuriGame <cena.kzscene> [GameModule.dll/.so]
```

Sem argumentos, procura por `Start.kzscene` na pasta atual. Os caminhos de
assets na cena são relativos ao diretório de trabalho.

**Download** — uma artifact por OS no Actions (combina engine + assembly
C#): `Kizuri-windows-latest.zip` ou `Kizuri-ubuntu-latest.zip`.
Extraia o zip — a estrutura é:

```
Kizuri/
├── bin/
│   ├── KizuriEditor.exe      (ou KizuriEditor no Linux)
│   ├── KizuriGame.exe        (ou KizuriGame no Linux)
│   ├── Sandbox.exe           (ou Sandbox no Linux)
│   ├── KizuriEngine.dll      (ou libKizuriEngine.so no Linux)
│   └── *.dll                 (runtime MinGW no Windows)
└── managed/
    ├── Kizuri.Scripting.dll
    ├── Kizuri.Scripting.deps.json
    ├── Kizuri.Scripting.pdb
    ├── SampleGame.dll
    ├── SampleGame.runtimeconfig.json
    └── SampleGame.deps.json
```

**Pré-requisito:** .NET 10 SDK (para compilar seu jogo) e .NET 10 runtime
(para o CoreCLR embutido na engine carregar o assembly).

**Seu jogo** é um projeto C# `Exe` que referencia `Kizuri.Scripting.dll`
(da pasta `managed/` do zip), herda `Kizuri.Script` e registra classes
com `GameModule.Register<T>(...)`. O `SampleGame.dll` já incluso no
zip serve de referência — ou compile seu próprio jogo:

```bash
dotnet build SeuJogo.csproj -c Release
```

---

## 🗂️ Estrutura

```
Kizuri-Engine/
├── engine/                       KizuriEngine (biblioteca SHARED)
│   ├── include/kizuri/
│   │   ├── core/                 Application, Window, Input, Layer, Log, ImGuiLayer...
│   │   ├── renderer/             Shader, Buffer, Texture, Camera, Renderer2D/3D, TextRenderer, Mesh
│   │   ├── ecs/                  Scene, Entity, Components
│   │   ├── scene/                SceneSerializer (.kzscene), Prefab, EditorHistory
│   │   ├── scripting/            CSharpBridge (.h) + NativeScript + ScriptEngine
│   │   ├── project/              Project (.kzproj), GameExporter
│   │   ├── audio/                AudioEngine (miniaudio)
│   │   └── assets/               AssetManager
│   └── src/                      implementação
├── editor/                       KizuriEditor — dockspace ImGui + ImGuizmo
├── managed/
│   ├── Kizuri.Scripting/         API pública C# do jogo (P/Invoke para o ABI 'kz_*')
│   └── SampleGame/               exemplo de jogo em C#
├── examples/
│   ├── sandbox/                  exemplo mínimo de jogo em C++
│   └── KizuriGame/               executável de jogo standalone
├── third_party_generated/glad/   bindings OpenGL 4.5 core (gerados, comitados)
├── cmake/                        scripts auxiliares (EmbedResource, etc.)
└── docs/                         NOTAS_INTERNAS.md (estado real do projeto)
```

---

## 📄 Licença

**Software proprietário** — todos os direitos reservados © 2026. Ver `LICENSE.md`.
Bibliotecas de terceiros permanecem sob suas respectivas licenças
(ver `THIRD_PARTY_LICENSES.md`).
