# Notas internas — Kizuri Engine

> **Uso interno, não é pra ir pro público.** Aqui vai o estado real do projeto, sem filtro:
> o que funciona, o que é gambiarra de v1, o que tá quebrado, e o que falta. O `README.md`
> na raiz é a versão pública/limpa — mantém as duas em sincronia quando algo mudar de status.

Última revisão: sessão de implementação de PBR/IBL → CSM → Bloom → Partículas → Áudio →
Play/Stop → diálogos nativos de arquivo → redesign do modal de GameModule →
**API C# de gameplay (runtime)** + **export self-contained** + **fix do ALC do Host**.

---

## 1. Como o projeto está organizado

```
Kizuri-Engine/
├── engine/                        KizuriEngine (biblioteca, SHARED/.dll)
│   ├── include/kizuri/
│   │   ├── core/                  Application, Window, Input, Layer, ImGuiLayer, FileDialog
│   │   ├── renderer/              Shader, Buffer, Texture, Camera, Renderer2D, Renderer3D, Mesh
│   │   ├── ecs/                   Scene, Entity, Components
│   │   ├── scene/                 SceneSerializer (.kzscene em JSON)
│   │   ├── scripting/             NativeScript, ScriptEngine, ScriptRegistry
│   │   ├── physics/                (Box2D 2D / Bullet3 3D, integrado direto no Scene)
│   │   └── audio/                 AudioEngine (miniaudio)
│   └── src/                       implementação de tudo acima
├── editor/                        KizuriEditor (.exe) — dockspace ImGui + ImGuizmo
├── examples/sandbox/               jogo de exemplo mínimo, roda OnUpdateRuntime puro
├── third_party_generated/glad/    bindings OpenGL 4.5 core (glad2), gerados e comitados
├── cmake/glad_stub/                MORTO — não referenciado em nenhum CMakeLists, pode apagar
└── .github/workflows/build.yml    build manual (workflow_dispatch), só Windows
```

Build: CMake + `FetchContent` (GLFW, glm, EnTT, spdlog, Bullet3, Box2D, miniaudio, stb,
tinyobjloader, ImGui, ImGuizmo, nlohmann/json). `glad` é a exceção — vem **pré-gerado e
comitado** em `third_party_generated/glad/`, não baixado via FetchContent, porque a geração
de verdade do glad2 precisa de Python rodando na hora do CMake configure, e isso já causou
dor de cabeça em CI antes.

---

## 2. O que tá implementado de verdade (não é lista de intenção)

### Renderer 3D
- **PBR completo** (Cook-Torrance: GGX + Smith + Fresnel-Schlick), metallic/roughness.
- **Multi-luz dinâmica**: até 16 luzes por frame (Directional/Point/Spot), com atenuação por
  alcance e cone. `LightComponent` no ECS — antes disso só existia um sol hardcoded, nem
  editável.
- **Normal mapping** via TBN calculado por derivada de tela (`dFdx`/`dFdy`) — não precisa de
  atributo de tangente na mesh.
- **IBL** (Image-Based Lighting): céu procedural (gradiente + sol, sem HDRI externo) →
  cubemap → convolução em irradiância difusa + pré-filtro especular GGX por rugosidade (5
  mips). Também usado como skybox.
- **Sombra em cascata (CSM)**, 3 faixas de distância que acompanham a câmera. Sem blend
  suave entre cascatas nem texel snapping ainda (pode ter corte visível/tremida sutil em
  cena de contraste forte ou câmera em movimento rápido — cosmético, não trava nada).
- **Bloom + tonemap HDR (ACES)**: cena renderiza num framebuffer `RGB16F` interno, bright-pass
  (limiar 1.1, joelho suave) → blur gaussiano separável ping-pong (meia resolução, 4
  iterações) → composição final ACES + gamma. Limiar/intensidade são constantes fixas no
  código, sem UI ainda.
- **Partículas GPU-instanced**: 1 `glDrawElementsInstanced` por emissor (`ParticleSystemComponent`),
  billboard sempre de frente pra câmera, sem textura (degradê radial procedural). A
  *simulação* (posição/vida) roda na CPU — "GPU-instanced" é sobre o desenho, não a
  simulação. Não escala pra centenas de milhares de partículas simultâneas (precisaria de
  compute shader).

### ECS / Scene
- `entt` por baixo. Componentes: Transform, SpriteRenderer, MeshRenderer, Camera, Light,
  ParticleSystem, AudioSource, Rigidbody2D/3D + colliders, NativeScript.
- `SceneSerializer`: roundtrip JSON pra arquivo (`.kzscene`) e **também em memória** (usado
  pelo Play/Stop).
- **`MeshRendererComponent`/`Material` NÃO são serializados** — nem pro arquivo `.kzscene`
  nem, originalmente, pro roundtrip em memória. Isso é uma lacuna real do Pipeline de Assets
  (mesh/textura não têm um "caminho" serializável ainda, só existem como `Ref<Mesh>`/`Ref<Texture2D>`
  em memória). `Scene::Copy` (usado pelo Play) contorna isso com uma cópia direta por UUID,
  fora do JSON — mas salvar a cena em disco ainda perde qualquer Mesh Renderer. **Isso é a
  próxima dívida técnica mais importante do projeto.**

### Play/Stop no editor
- Botão na toolbar do viewport. Play tira uma cópia inteira da cena (`Scene::Copy`) — a
  original nunca é tocada, editar durante o Play é seguro/descartável. Stop restaura o
  ponteiro pra cena original, sem precisar desfazer nada.
- Antes disso, o editor **não rodava física/scripts/partículas/áudio nunca** — só o
  `Sandbox` (executável separado) chamava `OnUpdateRuntime`. Isso já foi corrigido, mas vale
  lembrar que é recente; se algo relacionado a Play "nunca funcionou", é porque literalmente
  não existia até essa correção.
- Salvar/Salvar Como ficam bloqueados durante o Play (evita gravar a cópia efêmera por cima
  do arquivo real).

### Áudio
- `AudioEngine` (miniaudio) — **existia no código mas nunca era inicializado** (`Init()`
  nunca era chamado em lugar nenhum). Corrigido, ligado no ciclo de vida da `Application`.
- `AudioSourceComponent`: arquivo, loop, tocar-ao-iniciar, espacial (3D, atenuação por
  distância) ou fixo (música/UI), volume.
- Bug real corrigido: `LoadedSound` nunca chamava `ma_sound_uninit` (use-after-free em
  potencial). `AudioEngine::StopAll()` adicionado — necessário pro Stop do Play funcionar.
- Sem trigger por evento ainda (colisão, script) — só `PlayOnStart`. Fica pra quando existir
  sistema de eventos/KZScript.

### Scripting C# — API de gameplay (nova)
- **`Scene.CreateEntity` / `Entity.Destroy`** funcionam dentro do Play e do
  KizuriGame (`kz_set_active_scene` liga/desliga a cena ativa em
  OnRuntimeStart/Stop). Entidades criadas assim não têm corpo de física
  automático — pra corpo, usar `Scene.InstantiatePrefab` (que passa por
  `Scene::Instantiate` e registra física/scripts).
- **`Scene.Raycast2D`** usa `b2World::RayCast` (Scene::Raycast2D) — só existe
  durante o Play, quando o mundo Box2D está vivo.
- **Setters de sprite/texto/áudio** mutam o componente direto (sem recriar a
  entidade). Texto é world-space, igual ao editor.
- **`SaveSystem`** é 100% managed (System.Text.Json) — não passa pelo ABI.
- Exemplo de tudo: `managed/SampleGame/PlayerController.cs` (projéteis, HUD,
  save F5, raycast a cada 0.5s).

### Export self-contained
- `Exportar Jogo` com a checkbox ligada roda `dotnet publish -r <RID>
  --self-contained true` no `<Projeto>/Source/*.csproj` e entrega o runtime
  .NET embutido em `<out>/Game/`. O jogador final não precisa instalar nada.
- Requer: dotnet SDK na máquina do dev, `Source/*.csproj` no projeto ativo, e
  a raiz da engine achada subindo da pasta `bin/` (marcador
  `managed/Kizuri.Scripting/Kizuri.Scripting.csproj`). Sem isso, cai no
  fallback antigo (cópia da pasta do assembly).
- O publish é síncrono (UI congela ~10-60s). Próximo passo: rodar em thread e
  mostrar progresso no modal.

### Fix do ALC do Host (0 scripts registrados)
- `Host.InitializeGameModule` carregava o assembly do jogo com
  `Assembly.LoadFrom` (ALC default), mas o hostpolicy resolve os delegates
  num ALC coletável próprio. Resultado: `[GameEntryPoint]` registrava num
  `GameModule` diferente do consultado por `GetScriptCount` → "módulo
  carregado, 0 scripts, nenhum erro". Corrigido carregando no MESMO ALC do
  Host (`AssemblyLoadContext.GetLoadContext(typeof(Host).Assembly)`).
- Extra: `Host.s_LastInitError` + `GetLastInitError` (exceção nunca mais é
  engolida) e `ScriptEngine::LoadModule` falha de forma visível se 0 scripts
  forem registrados.

### Editor
- Dockspace ImGui + ImGuizmo (mover/rotacionar/escalar).
- **Toolbar do viewport completo**: Mover/Rotacionar/Escalar (atalhos W/E/R,
  estado ligado ao `m_GizmoOperation`) + alternância 2D/3D + Play/Stop no
  canto direito. Atalho **F5 = Play / Shift+F5 = Stop** (edge-detect, não
  conflita com digitação).
- Hierarquia, Inspetor (com painéis pra todo componente relevante — Mesh Renderer, Light,
  Particle System, Audio Source, Camera, Sprite Renderer), Console com filtro por
  categoria, Content Browser (existe, mas sem projeto aberto não mostra nada de útil ainda).
- **Diálogos nativos de arquivo/pasta** (`kizuri::FileDialog`, Windows via COM `IFileDialog`)
  nos 5 lugares que precisavam: Salvar/Abrir Cena, Novo/Abrir Projeto, Carregar GameModule.
  **Só Windows tem backend nativo** — Linux/macOS caem pro campo de texto manual (não trava,
  só não abre diálogo).
- Modal de GameModule redesenhado: cabeçalho com ícone (padrão dos outros painéis), status
  real de carregamento (verde = carregado + lista de scripts / vermelho = erro de verdade,
  não só log) — antes fechava o popup mesmo se o carregamento falhasse, sem avisar nada.

### Correções de bug "estruturais" (não são feature, mas valem registro)
- **Contexto GL/GLSL**: `ImGuiLayer` forçava GLSL 450 mesmo quando só rolava negociar GL 3.3
  — corrigido, `Window` expõe a versão real criada e os shaders se adaptam.
- **Crash "abre e fecha" em build SHARED** (funcionava em STATIC): `ImGui::CreateContext()`
  rodava dentro da engine (DLL), mas o editor tinha sua PRÓPRIA cópia estática do ImGui
  (via ImGuizmo), com seu próprio `GImGui` nunca inicializado — cada chamada `ImGui::` do
  editor operava sobre um contexto nulo. Corrigido com `ImGui::SetCurrentContext()`
  explícito no início do `EditorLayer::OnAttach`.
- **Mojibake nas caixas de erro fatal**: `MessageBoxA` interpretando strings UTF-8 do
  código-fonte pela codepage ANSI do Windows. Trocado por `MessageBoxW` com conversão
  explícita.
- **glad "stub" vs. glad de verdade**: o projeto tinha um loader OpenGL escrito à mão
  (`cmake/glad_stub/`) que só tinha os símbolos que o código usava até então — quebrou assim
  que o PBR/IBL precisou de cubemap/renderbuffer/etc. Trocado pelo glad2 de verdade (GL 4.5
  core, 623 extensões), pré-gerado e comitado.
- **Play mostrando tela preta**: `CameraComponent` nasce com `Type = Orthographic2D` por
  padrão (pensado pra HUD/UI), mas a cena default só tinha UMA câmera, marcada `Primary`, e
  nunca tinha seu `Type`/posição setados explicitamente. Resultado: `RenderScene3D` (usado
  no Play) nunca encontrava uma câmera `Perspective3D` primária válida, então `Renderer3D::EndScene()`
  nunca rodava — nada de skybox, nada de cubo, só a cor de limpeza do framebuffer (cinza bem
  escuro, fácil de confundir com preto). Corrigido: a câmera padrão agora nasce
  `Perspective3D`, posicionada em `(0,2,6)` com leve inclinação pra baixo, olhando de
  verdade pro cubo de exemplo (antes ela nascia na origem, mesma posição do cubo, sem
  rotação — "dentro" dele, olhando pra lugar nenhum).
- **Câmera invisível no viewport 3D**: não existia NENHUMA representação visual de uma
  `CameraComponent` selecionada — nem posição, nem direção. Adicionado `EditorLayer::DrawCameraGizmo()`:
  desenha uma pirâmide de frustum (tamanho fixo, só visualização) + seta de "frente", projetadas
  manualmente pra tela via `ImDrawList` (não passa pela GPU do `Renderer3D`, mais simples).
  Só aparece com uma entidade de câmera selecionada, e só no modo 3D do viewport.

---

## 3. Lacunas conhecidas, por prioridade

1. **Pipeline de import avançado** — reimport, previews em disco, geração de
   normais. Salvar MeshRenderer/texturas em `.kzscene` e caminhos relativos
   ao projeto **já funcionam**.
2. **CSM sem blend/texel snapping** — cosmético.
3. **Bloom sem UI de ajuste** — limiar/intensidade fixos no shader.
4. **Sem animação de esqueleto** (bones/skinning) — não iniciado.
5. **Scripting C# em produção**: o jogo já é um assembly .NET carregado por
   um host CoreCLR embutido (CoreCLRHost/hostfxr), com a API Unity-like
   `Kizuri.Scripting`. O projeto do jogo precisa ser `OutputType Exe` (com um
   `Main` vazio) — só aplicações geram o `.runtimeconfig.json`/`.deps.json`
   que o host usa pra inicializar. O que falta de produto: `dotnet publish`
   auto-contido no fluxo de exportação (hoje o export copia a pasta do
   assembly; se não publicar self-contained, o jogador precisa do .NET
   Runtime 8 instalado).
6. **Diálogo nativo de arquivo só no Windows** — Linux/macOS sem backend.
7. **`cmake/glad_stub/`** é lixo morto — pode apagar.

---

## 3b. Fluxo v1 pra criar e empacotar um jogo

1. Hub → Novo/Abrir projeto (cria `Assets/` + `Source/` com template C#).
2. Monte a cena (sprites, física, scripts C# no Inspetor).
3. Play no editor pra testar (colisões / spawn / troca de cena).
4. *Definir cena como inicial* + *Exportar Jogo...* → pasta jogável.
5. Ou rode `KizuriGame Start.kzscene Game/SampleGame.dll` na pasta exportada.

---

## 4. Coisas pra lembrar antes de mexer em código

- Comentários novos: **no máximo 2 linhas, só o que for importante** — isso foi pedido
  explicitamente numa sessão anterior. Não precisa reescrever comentário antigo que já
  existe, mas se for editar um arquivo que tem comentário antigo verboso na área que você
  tá mexendo, aproveita e enxuga.
- `Renderer3D` é uma classe 100% estática (sem instância) — todo estado é `static` dentro
  dela. Isso significa que só existe UM `Renderer3D` no processo inteiro; cuidado se algum
  dia isso precisar suportar múltiplas janelas/contextos GL simultâneos.
- Física/partículas/áudio só simulam de verdade dentro de `Scene::OnUpdateRuntime` — nunca
  em `OnUpdateEditor2D/3D`. Isso é intencional (edição livre de efeito colateral), mas
  significa que testar qualquer coisa dessas precisa apertar Play.
- `Scene::Copy()` é o mecanismo por trás do Play — se adicionar um componente novo com
  estado que precisa sobreviver ao Play, decidir: vai por serialização JSON (preferível,
  cobre save-to-disk de graça) ou por cópia direta tipo `MeshRendererComponent` (só se
  serialização JSON genuinamente não fizer sentido pro tipo de dado).
