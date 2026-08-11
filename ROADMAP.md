# Roadmap — Kizuri Engine

> Objetivo final: motor C++ 100% privado; o jogo é um assembly **C#**
> (`Kizuri.Scripting.dll`), conversando com a engine só por um ABI C. Nada
> de headers/libs internos expostos. (Antes era "GameModule em C++".)

> Objetivo da v1: engine **funcional e jogável** — criar no editor, scriptar
> em C# (assembly managed) e exportar um executável standalone.

> Objetivo da v0.2 ("Profissional"): transformar o protótipo numa engine de
> verdade — gráficos no ultra (mínimo OpenGL 3.3 core), pipeline de
> conteúdo, editor com ferramentas de qualidade — e crescer o pacote
> distribuído pra casa de centenas de MB a **~1GB** com um **Content Pack**
> real (modelos glTF, texturas PBR, HDRIs de céu, áudios, prefabs).

## Política de versionamento (0.x.y)

A versão da engine é `major.minor.patch` e o **major fica sempre 0** até o
minor chegar em `99` — quando seria `0.100`, vira **`1.0`**.

- **Upgrade GRANDE** (sistema novo, ferramenta nova, feature marcante) →
  sobe o **minor** e zera o patch: `0.30.0` → `0.31.0`.
- **Ajustes, correções e features pequenas** → sobem só o **patch**:
  `0.30.0` → `0.30.1`.
- Features pequenas **acumulam** até formarem um upgrade grande — aí sobem o
  minor de uma vez (não sobe versão a cada mudança pequena).
- O minor sobe de `0.x` até `0.99`; o próximo (seria `0.100`) vira **`1.0`**
  (a "100" é a versão 1.0).

> Changelog no Discord: só publicar quando o dono pedir — nunca automático.

---

## ✅ Base jogável (feito)

### Render e gameplay
- [x] Renderer 3D PBR + IBL + CSM + Bloom + partículas
- [x] Renderer 2D: sprites, círculos, grid, texto, tilemap, animação
- [x] Serialização MeshRenderer + material/texturas em `.kzscene`
- [x] Física 2D (Box2D) e 3D (Bullet3), colisões de tilemap
- [x] **Callbacks de colisão** `OnCollisionBegin/End` nos NativeScripts
- [x] **Instantiate** de `.kzprefab` em runtime (física + OnCreate)
- [x] **LoadScene** diferido a partir de scripts
- [x] Áudio (Play/Stop)
- [x] `SetLinearVelocity` / sync kinematic no Rigidbody2D

### API C# de gameplay (Kizuri.Scripting)
- [x] Entidades em runtime: `Scene.CreateEntity`, `Entity.Destroy`, `AddSprite/AddText/AddAudio/AddCamera`
- [x] `Scene.InstantiatePrefab`, `Scene.Load`, `Scene.GetPrimaryCamera`
- [x] Mutação runtime: sprite (textura/cor), texto (conteúdo/tamanho/cor), áudio (Play/Stop)
- [x] Input de mouse (botões + posição)
- [x] `Audio.PlayOneShot` / `Audio.StopAll`
- [x] `Scene.Raycast2D` (Box2D)
- [x] `SaveSystem` — persistência de jogo em JSON (puramente managed)

### Sistema de UI + utilidades de gameplay
- [x] **UI interativo**: `UICanvasComponent` + `UIRectComponent` + `UIButtonComponent` + texto
- [x] API C# de UI + serialização + painéis no Inspetor
- [x] **Corrotinas** estilo Unity: `StartCoroutine` + `WaitForSeconds`/`WaitForFrames`
- [x] **`Time.TimeScale`** · **`Rand`** · **`Mathf`**
- [x] **Sorting layers** 2D · **`CircleCollider2D`** · **`OverlapCircle2D`** · **`Duplicate`**

### Editor / produto
- [x] Hierarquia, Inspetor, Console, Content Browser, gizmos, Play/Stop
- [x] Salvar Prefab + arrastar `.kzprefab` pro viewport
- [x] **Exportar Jogo...** (pasta com KizuriGame + cena + assets + módulo)
- [x] Cena inicial do projeto (`.kzproj` → `StartScenePath`)
- [x] Caminhos de asset relativos ao projeto · `KizuriGame` com resize/troca de cena

---

## ✅ v0.2 — Profissional (feito)

### Gráficos no ultra — mínimo **OpenGL 3.3 core**
- [x] **`GraphicsSettings` + presets** Ultra/High/Medium/Low/Custom: resolução
  interna (render scale), MSAA, tamanho do shadow map, raio do PCF, bloom,
  SSAO, exposição e VSync — aplicado em runtime sem reiniciar
- [x] **MSAA no framebuffer HDR** (multisample + blit resolve cor/profundidade)
- [x] **SSAO** (oclusão de ambiente em espaço de tela): kernel de hemisfério
  de 64 amostras + ruído 4x4, meia resolução + blur separável
- [x] **HDRI / equirect IBL**: `Renderer3D::SetEnvironmentHDRIPath` carrega
  `.hdr`, converte pra cubemap e rebakeia irradiância + pré-filtro GGX
  (fallback procedural se falhar)
- [x] **Exposição** no composite (antes do tonemap ACES) · bloom e PCF
  configuráveis · **2D 100% GLSL 330** (batching por textura — fim do 450)

### Pipeline de conteúdo
- [x] **Import glTF/glb** via cgltf (`Mesh::LoadFromGLTF`): primitivas
  triangulares, indices 16/32-bit; `FromSource` roteia .obj/.glb/.gltf
- [x] **`content/` = Content Pack**: `models/Cube.glb` (1º asset real, ~1.8KB)

### API C# 3D profissional
- [x] `SetRotation` / `SetScale` (Transform completo em runtime)
- [x] `AddLight` (direcional/ponto/spot) + `SetLightColor`/`SetLightIntensity`
- [x] `AddMeshRenderer` + `SetMaterial` (albedo/metallic/roughness + mapas)
- [x] `SetCamera` (FOV/clipes) · `ComponentType` += MeshRenderer/ParticleSystem
- [x] `Demo3D.cs` (sol + luz pontual + cubo PBR girando) no SampleGame

### Editor Pro
- [x] **Configurações Gráficas** (Arquivo): presets, MSAA, SSAO, bloom, HDRI
  do céu — persistido em `settings.json`, aplicado ao vivo
- [x] **Gizmo de luz** no viewport 3D (ponto/spot/direcional)
- [x] **Previews reais** de imagem no Content Browser (cache por caminho)
- [x] **Drop de assets** do Content Browser nos slots de Malha/Albedo/Normais

---

## ✅ v0.6 — Conteúdo embutido (`kzres://`) + fixes de Play (feito)

- [x] **Recursos embutidos no .exe/.dll**: `cmake/EmbedContent.cmake` gera um
  registro name->bytes compilado no binário; qualquer slot de asset aceita
  `kzres://<nome>` (mesh, textura, skin, HDRI). Atual: `models/Cube.glb` e
  `skies/sky_gradient.hdr` (HDRI procedural) — a demo 3D tem céu embutido
- [x] **Fix Play — câmera com gimbal lock**: RenderScene3D decompunha a matriz
  do transform via `glm::eulerAngles`; em yaw = ±90° (singularidade) extraía
  pitch ±180° e a câmera olhava pra trás. Agora a orientação é lida direto do
  euler local do TransformComponent (à prova de gimbal lock)
- [x] **Fix Play — texturas cinza**: mapas EMBUTIDOS no .glb (sem caminho de
  arquivo) se perdiam ao desserializar (Play/cópia/save/undo) → material
  cinza. `RestoreGLTFTextureMaps()` reextrai do próprio .glb só os mapas
  vazios na recarga

---

## ✅ v0.7 — Usabilidade (feito)

- [x] **Fullscreen do viewport**: botão de ícone na toolbar + F11 — esconde
  os painéis laterais e o viewport ocupa a janela inteira
- [x] **Responsividade**: a janela é limitada à área de trabalho do monitor
  primário e centralizada (não fica cortada em telas pequenas)
- [x] **Configurações** (Arquivo > Configurações): janela com sidebar e
  seções Gráficos / Geral / Editor — no lugar da aba padrão do ImGui
- [x] **Cena de Demonstração 2D**: sprites, física Box2D (chão + caixas
  caindo), moedas, texto e UI (canvas + botão) — roda 100% no Play
- [x] **Jogo 100% 2D ou 100% 3D**: o conteúdo padrão de cena respeita o modo
  do projeto (2D = câmera ortográfica; 3D = perspectiva). O Play renderiza o
  passe 2D só se há câmera primária ortográfica e o 3D só se há câmera de
  perspectiva; abrir projeto recria a cena por modo (ou carrega a inicial)

---

## ✅ v0.3 — Produção 3D (feito)

### Animação esquelética (skinning via glTF)
- [x] **SkinData/AnimationClip**: parse de skins + animações do `.glb/.gltf`
  (juntas, inverse bind, hierarquia topológica, canais TRS com lerp/slerp)
- [x] **SkinnedMesh**: Vertex3D com Joints/Weights, shader com até 64 juntas,
  `Renderer3D::SubmitSkinned`
- [x] **AnimatorComponent** serializado (path/clip/loop/speed/time) + skin
  recarregada sob demanda; `Scene::UpdateAnimators` roda em edição (preview)
- [x] Editor: painel **Animador** (seletor de clip, play/pause, time scrubber)
- [x] C# API: `AddAnimator`, `PlayAnimation`, `AnimationTime`,
  `SetAnimationTime/Speed/Loop/Playing`

### Física 3D em C# (Bullet3)
- [x] `AddRigidbody3D` (estático/dinâmico/cinemático), `AddBoxCollider3D`,
  `AddSphereCollider3D`, `ApplyForce`, `ApplyImpulse`, `TryGetVelocity`,
  `SetVelocity`
- [x] Registro lazy de corpos: entidades criadas em runtime ganham corpo no
  primeiro frame do UpdatePhysics (2D e 3D)

### Primitivas 3D + conteúdo
- [x] Novos builtins: `cylinder`, `cone`, `capsule`, `torus` (procedurais)
- [x] **Content Pack na CI**: job `package` baixa modelos glTF CC0 (Fox,
  BoxAnimated, DamagedHelmet, Avocado, BoomBox, Duck, CesiumMilkTruck,
  SimpleMaterials — o Fox/BoxAnimated exercitam o skinning) + HDRIs Poly
  Haven (céu HDRI/IBL) direto pro artifact, com manifest — best-effort

---

## ✅ v0.4 — Materiais & Atmosfera (feito)

- [x] **Material completo**: `Emissive` + `EmissiveStrength`, mapa
  `MetallicRoughness` (G=roughness, B=metallic, convenção glTF) e mapa
  emissivo — serializados, com drop no Inspetor e API C#
  (`SetMaterialEmissive`, `SetMaterialMetallicRoughnessMap`)
- [x] **Fog exponencial** por distância no shader de mesh (`GraphicsSettings`:
  FogEnabled/Density/Color, presets + `settings.json` + UI)
- [x] **`Texture2D::CreateFromMemory`** (bytes em memória, sem flip — UV glTF):
  base pra extrair texturas embutidas de .glb
- [x] **`Mesh::ExtractMaterialFromGLTF`**: fatores PBR + texturas embutidas do
  modelo; drop de .glb (viewport ou Inspetor) aplica o material
  automaticamente
- [x] **Content Pack**: +BrainStem/CesiumMan/RiggedFigure (esqueléticos),
  +HDRIs 4k e +texturas PBR ambientCG 2K (6 conjuntos) — **artifact ~200MB**

---

## ✅ v0.5 — Showcase & Ferramentas (feito)

- [x] **Cena de Demonstração 3D** (Arquivo): Fox esquelético animado (Survey),
  DamagedHelmet PBR com texturas reais, primitivas (torus metálico + esfera
  emissiva → bloom), HDRI de céu, fog e 2 luzes — cai pros builtins sem o pack
- [x] **Gizmos de colisor** no viewport (debug-draw verde): círculo/box 2D e
   box/esfera 3D da entidade selecionada

---

## ✅ v0.8 → v0.11 — Profissional & Jogo 3D (feito)

### v0.8 — Usabilidade
- [x] Fullscreen do viewport (botão + F11) · janela responsiva ao monitor
- [x] Configurações com sidebar (Gráficos/Geral/Editor) · menubar organizada
  (Arquivo/Editar/Cena/Exibir/Ajuda) · botões "..." de arquivo nativo em todos
  os campos de asset · cena demo 2D · jogo 100% 2D/3D (conteúdo por modo)
- [x] Versão 0.8.0 · GLSL por hardware (330/400/430/450) + `kzres://` embutido

### v0.9 — Jogo 3D de verdade
- [x] `Scene.Raycast3D` (Bullet) · `Scene.OverlapSphere3D` (sensor fantasma)
- [x] `Rigidbody3D.ApplyTorque/TryGetAngularVelocity/SetAngularVelocity`
- [x] `Entity.Name` (get/set) · `Time.time/unscaledTime/unscaledDeltaTime`

### v0.10 — Ferramentas profissionais
- [x] Busca na hierarquia (filtra por nome, lista plana)
- [x] Cena de demonstração 2.5D (3D + 2D + UI com as duas câmeras)
- [x] Content pack maior (SimpleSkin, ReciprocatingSaw, Character + 4 texturas PBR)

### v0.11 — Acabamento & Controle
- [x] Tonemapping selecionável (ACES/Reinhard/Filmic)
- [x] `Mathf.SmoothStep/PingPong/Remap/LerpAngle/Angle`
- [x] `Scene.InstantiatePrefab(path, pos, rotation)`

### Render (todas as versões)
- [x] Céu atmosférico escuro (pôr-do-sol + estrelas, IBL estável) · pós-cinema
  (vinheta/CA/grão) · SSAO com teto de escurecimento · PCSS + sombra de luz
  pontual **versão-gated** (GL 4.0+) — 3.3 mantém o PCF simples estável

### v0.12 — Mundo mais vivo
- [x] Partículas com textura (componente + serialização + Inspetor + C#)
- [x] Sombra ANIMADA: o passe de sombra aplica skinning (pose animada do Fox)

### v0.13 — Controle 3D
- [x] `Entity.TryGetWorldPosition` (respeita hierarquia) · `Entity.LookAt(target)`

### v0.14 — Sons 3D
- [x] `Audio.PlayOneShotAt(path, vol, position)` — one-shot posicional (pool
  seguro de ma_sound: end-callback marca o slot; uninit no main thread)

### v0.15 — Content Browser pro
- [x] Renomear arquivo (menu de contexto + modal) · ícones coloridos por tipo

### v0.16 — Física fina & 2D
- [x] `Rigidbody2D.GravityScale` (<0 invertida, 0 sem gravidade) — aplicada no
  corpo e em runtime · `Sprite.FlipX/FlipY` (espaço local, serializado)

---

## ✅ v0.17 — Self-contained de verdade (assets padrão EMBUTIDOS)

> Motivação: a engine não pode depender do Content Pack pra funcionar — se o
> usuário apagar um arquivo, nenhuma função padrão pode quebrar. Os assets
> que a engine usa como padrão (demonstrações, céu, modelos) passam a vir
> DENTRO do executável via `kzres://`, e a "pesada" da engine vem de
> **features**, não de um pacote solto.

- [x] CI baixa Fox.glb (skinned + animado) e DamagedHelmet.glb (PBR) no build
  e o CMake os EMBUTE no binário (manifest `EmbedContent` + `kzres://`)
- [x] Prioridade de asset: **embutido (`kzres://`) → Content Pack → builtins** —
  a demo 3D nunca fica sem o Fox/capacete, mesmo se `../content` sumir
- [x] `Mesh::ExtractMaterialFromGLTF` aceita `kzres://` (parse em memória) +
  `ExtractMaterialFromGLTFMemory`
- [x] `LoadGLTFTexture` decodifica data-URI base64 (alguns .glb embutem imagem
  como base64 em vez de buffer view) — material nunca mais perde textura
- [x] Céu padrão da demo: Content Pack → **`kzres://skies/sky_gradient.hdr`**
  (embutido) → procedural atmosférico
- [x] Cenas padrão 2D/3D, demos 2D e 2.5D: 100% builtins (sem dependência externa)

---

## ✅ v0.18 — Reflexos por raio (SSR) + Configurações cheias

- [x] **SSR — "ray tracing" em espaço de tela** (GL 4.0+): marcha o raio
  refletido contra o depth buffer e compõe a cor do impacto como reflexão —
  piso espelhado na demo 3D. Em 3.3 fica off (o loop de comprimento variável
  quebrava shader no Wine), gated por `GetGLSLVersion() >= 400`
- [x] `GraphicsSettings`: SSR ligado/desligado, passos do raio, intensidade,
  distância da marcha, espessura do depth — com presets, `TuneToHardware()`
  e persistência em `settings.json`
- [x] Passe SSR no pipeline (cor+depth → RGBA16F → composição HDR antes do
  tonemap), com fresnel aproximado (ângulos rasantes refletem mais)
- [x] **Configurações > Geral**: resolução editável + aplicar, maximizar/
  minimizar, VSync, info de OpenGL resumida
- [x] **Configurações > Editor**: velocidade da câmera livre, sensibilidade do
  mouse, snap de translação e rotação dos gizmos, demo 2.5D
- [x] **Configurações > Gráficos**: grupo de reflexos por raio (SSR)

---

## ✅ v0.19 — Carregamento incremental (projetos grandes sem travar)

- [x] Bug: abrir projeto/cena muito grande congelava o editor (deserialize
  síncrono no main thread: mesh/textura/skin carregavam tudo de uma vez —
  janela "não responde" e não fechava)
- [x] `SceneSerializer` ganhou API INCREMENTAL: `BeginDeserializeStepwiseFile`
  + `StepDeserialize(maxEntidades)` + `StepDeserializeTime(orçamento)` —
  processa lotes por frame com progresso; hierarquias resolvidas no fim
- [x] `OpenScene` e cena inicial do projeto viram assíncronos: cada OnUpdate
  consome ~4ms de trabalho, overlay de progresso centrado, loop de eventos
  continua vivo (janela fecha normalmente a qualquer momento)
- [x] `Scene.Load` em runtime (Play) também usa o carregamento assíncrono —
  a conclusão religa o runtime da cena nova
- [x] `Deserialize()`/`DeserializeFromJson()` mantêm o comportamento síncrono
  (Prefab e snapshot de Play/Stop não mudam)

---

## ✅ v0.20 — Viewport preto em OpenGL 4.x (primeiro teste em GPU 4.6 real)

- [x] Bug: tela preta no PC da escola (GL 4.6). Causa: o SSR (novo, só roda em
  4.0+) reconstruía a normal por dFdx/dFdy — em silhuetas/bordas a derivada é
  ~0, `normalize(0)` = NaN, e o NaN propagava pro composite (tela preta).
  No 3.3 o SSR não roda, por isso nunca apareceu antes.
- [x] SSR: descarta pixels de normal degenerada (dot(n,n)<1e-8), sanitiza a
  cor do impacto e clampeia a saída — nunca mais emite NaN
- [x] Composite: guarda anti-NaN no reflexo + guarda FINAL (nenhum passe de
  pós pode apagar a tela com NaN/Inf)
- [x] Bright-pass: guarda anti-NaN (bloom não propaga)
- [x] Shader::IsValid(): shader que falha ao vincular não roda mais — SSR
  desligado silenciosamente se o driver rejeitar (em vez de frame quebrado)

---

## ✅ v0.21 — Viewport preto em 4.x: fallback de versão GLSL + contexto 4.5 + diagnóstico na tela

- [x] Suspeita: drivers que anunciam GLSL 4.60 mas rejeitam `#version 460`
  (Mesa/llvmpipe, VMs, iGPU antigas) — TODOS os shaders falhavam = tudo preto
- [x] Shader: fallback automático de versão — tenta 460→450→430→410→400→330
  (o 330 é o caminho comprovado; features 4.x degradam via `#if`). Shader que
  um driver rejeite numa versão compila na anterior em vez de quebrar o frame
- [x] Contexto: cadeia agora tem TETO 4.5 (o glad é 4.5; 4.6 não agrega nada e
  é onde os drivers mentem sobre GLSL)
- [x] Diagnóstico NA TELA: faixa vermelha no viewport com a última falha de
  shader/FBO/GL (SetShaderDiagnostic) — sem precisar caçar o log
- [x] FBOs incompletos (HDR/MSAA/bloom/SSAO/SSR) e glGetError viram aviso na
  tela + fallback de MSAA (clampa no driver e desliga se falhar)

---

## ✅ v0.22 — Engine 100% OpenGL 3.3 (fim da saga do viewport preto)

- [x] Decisão: a engine usa SEMPRE OpenGL 3.3 core, em qualquer máquina —
  o caminho comprovado e estável (Wine, iGPU, VM, driver novo) — acabou o
  viewport preto de drivers que anunciam GLSL 4.x
- [x] Contexto: cadeia agora pede só {3,3}; teto GLSL 330
- [x] Features gated por GLSL>=400 (SSR/PCSS/sombra de luz pontual) ficam
  sempre desligadas (compiladas fora no #if, shader SSR não é criado)
- [x] MSAA 4x, shadow 2048, PCF 2, SSAO 32, bloom 4 — os valores 3.3 provados
- [x] Os gráficos "ultra" futuros virão do backend VULKAN (em andamento)

---

## ✅ v0.23 — Removido TUDO de OpenGL > 3.3 (limpeza final)

- [x] **PCSS removido** do mesh shader — volta ao PCF simples comprovado (o
  bug de "objetos brilhando branco absurdo" era dos loops dinâmicos 4.x)
- [x] **Sombra de luz pontual removida** (shaders + FBO cubemap + passe 6
  faces + uniforms + campos de settings) — luzes de ponto iluminam sem sombra
- [x] **SSR (reflexos por raio) removido inteiro** (shader, FBO, passe,
  uniforms do composite, campos de settings, UI)
- [x] `Shader`: sem escalonamento por versão — #version 330 core fixo, sem
  KZ_GLSL_VERSION e sem fallback de versões
- [x] `GraphicsSettings`: sem `ShadowSoftness`/`PointShadowMapSize` e sem os
  campos de SSR (mortos); `TuneToHardware` usa UM único conjunto 3.3
- [x] UI do editor sem os sliders de PCSS/sombra pontual/SSR
- [x] Resultado: a engine é 100% OpenGL 3.3 — um único caminho comprovado.
  Mesmo num PC com OpenGL 4.5/4.6 o contexto pedido é {3,3} e todos os
  shaders são GLSL 330 core.

---

## 🚧 v0.24 — Extraindo o MÁXIMO do OpenGL 3.3 (rumo a gráficos AAA)

> A engine roda 100% em OpenGL 3.3 core, mas isso não é limite de qualidade:
> GLSL 330 compila loops de passos CONSTANTES, então técnicas "próximas" de
> reflexos/soft-shadow/AO que antes exigiam 4.x voltam reescritas 3.3-safe.

### ✅ Reflexos por raio (SSR) 3.3-safe
- [x] SSR reimplementado com marcha de loop de PASSOS FIXOS (`#define
  SSR_MAX_STEPS 48` constante no shader) — unrollável, compila em qualquer
  driver GL 3.3 (Wine/iGPU/VM). O SSR antigo usava loop de comprimento
  variável (só 4.x) e por isso tinha sido removido
- [x] Pipeline: cor + depth resolvidos → RGBA16F → somado ao HDR no composite
  antes do tonemap, com Fresnel (ângulos rasantes refletem mais)
- [x] Guardas anti-NaN no shader (normal degenerada descartada) e no composite
  (reflexo envenenado nunca apaga a tela)
- [x] `GraphicsSettings`: SSR ligado/desligado, passos, intensidade, distância
  da marcha, espessura do depth — com presets, persistência em `settings.json`
  e UI no editor (Configurações > Gráficos)

### ✅ Anti-aliasing temporal (TAA) — imagem "de filme"
- [x] Jitter da câmera por frame em sequência **Halton** de baixa discrepância
  (±1px em cada eixo), aplicado ANTES de renderizar a cena — cor, depth, SSAO
  e SSR ficam todos consistentes com a mesma projeção jitterada
- [x] Passe fullscreen: mistura do frame atual com o histórico anterior com
  **clamp em AABB** (vizinhos 3x3 do frame atual) — sem vazar cores/ghosting
- [x] Alvo intermediário do composite + 2 históricos RGBA8 ping-pong; blit
  pro destino final (upscale/upsample na mesma qualidade de antes)
- [x] Histórico invalidado automaticamente em resize/troca de alvo (sem
  manchas residuais); primeiro frame usa o atual puro
- [x] `GraphicsSettings`: TAA ligado/desligado — presets (Low desliga) +
  `settings.json` + UI no editor

### ✅ Parallax Occlusion Mapping (POM) — superfície com relevo real
- [x] Marcha de **passos fixos** (`#define POM_MAX_LAYERS 24` constante) sobre
  o mapa de altura ao longo da visão em espaço tangente (base TBN por derivada
  de tela) + interpolação linear entre os dois últimos passos — 100% GLSL 330
- [x] Coordenada de textura deslocada ANTES de amostrar albedo/normais/MR/
  emissivo — o relevo acompanha o ângulo da câmera (nada de "textura lisa")
- [x] Material: `HeightMap` + `HeightScale` — serializados no `.kzscene`
  (`HeightMapPath`/`HeightScale`), drop/input no Inspetor do editor
- [x] C# API: `SetMaterialHeightMap(path)` + `SetMaterialHeightScale(scale)`
  (`kz_material_set_height_map` / `kz_material_set_height_scale` no bridge)
- [x] Câmera olhando de lado (Vt.z < 0.02) desliga o POM sem artefato

### ✅ Céu atmosférico Rayleigh/Mie (scattering físico)
- [x] Skybox procedural com **single-scattering**: marcha do raio de visão na
  atmosfera + marcha da luz do sol até cada ponto, com transmitância e fases
  de Rayleigh (azul) e Mie (haze/halo) — loops de TETO CONSTANTE, 100% GLSL 330
- [x] Unidades NORMALIZADAS (raio do planeta = 1.0) com β e scale-height
  re-escalados — sem estourar a precisão float32 na interseção raio-esfera
- [x] Sol segue a luz direcional (o disco bate na mesma direção da sombra) +
  disco HDR (>1 alimenta o bloom) + halo quente Mie + estrelas celulares no
  lado noturno
- [x] HDRI carregado = continua mostrando o ambiente do usuário; sem HDRI o
  céu passa a ser o scattering (o pôr-do-sol laranja realista)
- [x] Guardas anti-NaN no skybox

### ✅ PCSS — sombras suaves de verdade (passos fixos, 3.3-safe)
- [x] Percentage-Closer Soft Shadows reimplementado com loops de TETO
  CONSTANTE (`#define PCSS_MAX_RADIUS 5`): busca de bloqueadores → penumbra
  proporcional à distância do bloqueador → PCF dentro da penumbra. O raio
  fixo com `continue` só descarta taps — o loop é unrollável no GLSL 330
  (o PCSS antigo de raio dinâmico era o que quebrava em Wine → objetos
  brancos; por isso tinha virado 4.x)
- [x] `ShadowSoftness` (0..1, 0 = desliga e cai no PCF simples) — presets,
  `settings.json` + UI no editor ("Penumbra (PCSS)")

### ✅ God rays / luz volumétrica (espaço de tela)
- [x] Marcha RADIAL do pixel até a posição do sol na tela, acumulando a cor
  brilhante da cena com density/decay — loop de TETO CONSTANTE
  (`#define GODRAY_MAX_STEPS 28`), 100% GLSL 330
- [x] Sol segue a luz direcional; pass só roda com o sol (quase) dentro da
  tela; meia resolução + blur horizontal pra suavizar; composto no composite
  antes do tonemap com guarda anti-NaN
- [x] `GodRaysEnabled` + `GodRaysIntensity` — presets (Ultra/High ligam),
  `settings.json` + UI no editor

### ✅ Reflexões planares (espelho real)
- [x] Pré-passe: a cena é renderizada de NOVO numa câmera refletida na face do
  espelho (posição espelhada + `view * reflectionMatrix`), com near plane
  OBLÍQUO (Lengyel) pra geometria do outro lado do espelho não "vazar"
- [x] O material espelhado (`PlanarReflect` no material, checkbox no Inspetor)
  projeta o próprio fragmento na VP da câmera refletida e amostra a textura
  RGBA16F — reflexo nítido sem tintar com o albedo; edge de Fresnel reforça a
  borda; guarda anti-NaN
- [x] O espelho não reflete a si mesmo (evita recursão); 1 espelho por frame;
  câmera atrás do espelho desliga o passe
- [x] Serialização `PlanarReflect` no `.kzscene`; 100% OpenGL 3.3 (FBO padrão)

### ✅ DOF (bokeh) + Motion blur — pós-cinema
- [x] **DOF em UM passe** (gather): círculo de confusão pela distância ao plano
  focal (reconstruída do depth) + disco de **Poisson FIXO** (20 taps,
  constante no shader) — frente/fundo desfocam com bokeh, amostras em foco
  pesam mais
- [x] **Motion blur por REPROJEÇÃO** (sem velocity buffer): ponto do mundo
  reconstruído pelo depth, projetado com a VP do frame ANTERIOR (guardada a
  cada frame, sem jitter do TAA) e do atual; blur linear ao longo do vetor de
  movimento (taps fixos)
- [x] Cadeia: HDR → DOF → Motion blur → bright-pass/composite (fonte única)
- [x] `DOFEnabled`/`DOFFocusDistance`/`DOFFocusRange`/`DOFStrength` +
  `MotionBlurEnabled`/`MotionBlurIntensity` — presets (Ultra/High ligam),
  `settings.json` e UI no editor; guardas anti-NaN

---

## 🎯 v0.24 completo — o que o OpenGL 3.3 entregou
- Reflexos (SSR 3.3-safe) · AA temporal (TAA) · Parallax Occlusion Mapping
- Céu Rayleigh/Mie · PCSS de passos fixos · God rays volumétricos
- Reflexões planares (espelho real) · DOF bokeh · Motion blur
- Tudo com loops de TETO CONSTANTE — 100% compilável em GLSL 330 core em
  qualquer driver (Wine, iGPU, VM), sem os bugs de drivers 4.x

---

## ✅ v0.25 — Mais do que o OpenGL 3.3 aguenta (parte 2)

- [x] **SSGI — iluminação global 1-bounce** (meia resolução): raios do
  hemisfério (8 direções fixas) marchados contra o depth coletando a cor
  indireta da cena — luz "quicada" real entre objetos
- [x] **Nuvens volumétricas** no céu atmosférico: raymarch por uma camada
  esférica com densidade de fbm 3D (4 oitavas, passos fixos), iluminadas pelo
  sol, com deriva lenta no tempo
- [x] **Lens flare** em espaço de tela: ghosts simétricos + streak horizontal
  a partir do brilho do bloom, na direção do sol
- [x] **FXAA** — AA de pós-processamento (3 taps clássico), alternativa/
  complemento ao TAA
- [x] **Color grading**: saturação e contraste no composite (pós-gamma)
- [x] **Bloom anamórfico**: blur horizontal alongado (streaks de cinema)
- [x] **Névoa por altura**: a névoa exponencial fica forte abaixo do plano e
  some acima (falloff configurável)
- [x] Todos os novos passes com loops de TETO CONSTANTE — 100% GLSL 330 core;
  `GraphicsSettings` completo (presets + `settings.json` + UI do editor)

---
## ✅ v0.26 — Editor completo: painéis dockáveis (em andamento, local)

> Este update fica LOCAL até o GitHub Actions voltar — nada de push ainda.

- [x] **Framework de painéis** (`editor/src/UI/Panels/`): classe base
  `EditorPanel` + `EditorContext` (contexto compartilhado preenchido pelo
  EditorLayer todo frame). Cada painel é um arquivo próprio — o EditorLayer
  só cria, preenche contexto e chama render/update (edits ADITIVOS, sem
  reescrever as 4400 linhas existentes)
- [x] Menu **Janelas** na menubar: liga/desliga cada painel
- [x] **Profiler** — painel dockável com FPS, tempo de frame (gráfico
  rolante), draw calls, triângulos e contagem de entidades
- [x] **Game View** — aba que mostra o JOGO rodando no Play (2º framebuffer
  via `Scene::RenderRuntimeView()`, sem rodar lógica duas vezes)
- [x] **Material Editor** — janela de material com preview ao vivo em esfera
  (renderizado num FBO próprio) + edição de cores/PBR/emissivo/POM/espelho
- [x] **Animator** — painel dedicado: clips, play/pause, loop, velocidade e
  time scrubber (substitui a UI espremida no Inspetor)
- [x] **Project Settings** — painel dockável com sidebar (Gráficos, Geral,
  Editor, Sobre): presets, MSAA/sombras/PBR/pós, HDRI do céu, janela,
  preferências do editor

---EOF## 🚧 v0.27 — Jogabilidade & Produto (em andamento)

> Escopo completo (por rodadas, cada uma vira commit + CI): Game View ao vivo
> em edição · Tags & Layers com filtro de colisão · Instancing de malhas ·
> Input Actions com rebind · Build settings (nome/ícone/versão/resolução) ·
> Prefabs aninhados + edição isolada · Multi-seleção + undo/redo · Audio
> mixer · Terrain · Timeline/Sequencer · LODs/occlusion · Profiler GPU ·
> loading entre cenas.

### ✅ R1 — Game View ao vivo em edição
- [x] O painel Game View renderiza SEMPRE (edição e Play): mostra a visão da
  **câmera principal** da cena ao vivo — edite no viewport e veja como fica
  pro jogador sem apertar Play
- [x] Botão "Focar câmera": seleciona a câmera principal no Inspetor
- [x] Render extra continua com TAA/MotionBlur desligados (ScopedTemporalOff)

### ✅ R2 — Tags & Layers (filtro de colisão)
- [x] `TagComponent` ganhou `Layer` (0..15) + `CollisionMask` (bits das
  camadas que colidem) — serializado no `.kzscene` e editável no Inspetor
- [x] Box2D (2D): `b2Filter` com category/mask por entidade
- [x] Bullet (3D): `setCollisionGroup/setCollisionMask`
- [x] C#: `Entity.Layer`, `Entity.CollisionMask`,
  `Entity.SetCollideWithLayers(params int[])`

### ✅ R3 — Audio mixer
- [x] Grupos de volume **SFX / Música / UI** (`AudioEngine::SetGroupVolume`)
  aplicados como multiplicadores em Play/PlayOneShot/PlayOneShotAt
- [x] `AudioSourceComponent.Group` (combo no Inspetor) + serializado
- [x] C#: `Audio.MusicVolume`, `Audio.SFXVolume`, `Audio.UIVolume`

### ✅ R4 — Build settings (export)
- [x] `ProjectConfig`: `GameName`, `Version`, `WindowWidth/Height` — persistidos
  no `.kzproj` e editáveis no modal Exportar Jogo
- [x] Export grava `game.json`; o KizuriGame lê na abertura e usa o
  nome + resolução da janela do jogo

### ✅ R5 — Input Actions (rebind)
- [x] `Input::IsActionPressed(name)` / `SetActionKey(name, key)` com mapa
  nome->tecla + ações padrão de gameplay
- [x] C#: `Input.IsActionPressed`, `Input.SetActionKey`, `Input.GetActionKey`

### ✅ R6 — Multi-seleção (editor)
- [x] Ctrl+clique na Hierarquia seleciona várias entidades (destaque conjunto)
- [x] Inspetor mostra "N entidades selecionadas" + "Excluir seleção"
- [x] Excluir no menu opera em todas as selecionadas; seleção resetada com o estado

### ✅ R7 — Profiler: GPU e memória
- [x] Profiler mostra a GPU (renderer OpenGL) e a RAM do processo
  (Linux via /proc/self/statm)

### ✅ R8 — Instancing de malhas
- [x] `Renderer3D::SubmitMeshInstances` — mesma malha/material em N transformadas
  num ÚNICO draw call (uniform array + `gl_InstanceID`, 128/lote, GLSL 330-safe)
- [x] Render no EndScene após as malhas; `u_Instanced` sincronizado nos passes
- [x] C#: `Scene.DrawInstanced(mesh, cor, transforms, count)` +
  `Scene.MakeTransform(x, y, z, scale)` (floresta/multidão em runtime)

### ✅ R9 — Fade de troca de cena (jogo)
- [x] KizuriGame escurece (fade in), troca a cena e clareia (fade out) —
  troca via `Scene.Load` fica suave em vez de cortar seco

---
## 🚧 v0.28 — Experiência & Produto (em andamento)

- [x] **Preset padrão = Médio** (antes Ultra): abre em qualquer máquina;
  suba o preset no editor se aguentar
- [x] **Game View ≠ Viewport**: no Play o VIEWPORT usa a CÂMERA DO EDITOR
  (voar pela cena) e o GAME VIEW mostra a câmera do JOGADOR — sem redundância;
  Game View renderiza só no Play (o "ao vivo" em edição foi removido)
- [x] **Configurações unificadas**: só existe o painel **Project Settings**
  (menu Configurações... abre ele); a janela antiga "Configurações" foi
  descontinuada; seção Áudio (mixer) adicionada
- [x] Requisitos mínimos/recomendados documentados (preset Médio padrão)

### ✅ Frustum culling (sistema de engine)
- [x] Culling dos 6 planos (Gribb-Hartmann) no passe 3D: malhas fora da câmera
  não são submetidas (AABB do mundo dos 8 cantos) — economiza vértices/pixels;
  esqueletos não são culled (pose animada pode sair da AABB de repouso)
- [x] Funciona no viewport (câmera do editor) e no Play (câmera primária)

### ✅ LOD — Level of Detail (sistema de engine)
- [x] `LODComponent`: vários níveis (malha + distância), trocados pela
  distância à câmera no passe 3D; `DistanceMultiplier` pra afinar tudo
- [x] Serializado no `.kzscene`, UI no Inspetor (adicionar/remover/ordenar
  níveis) e menu Adicionar Componente
- [x] Sem níveis = comportamento normal; esqueletos ignoram LOD

### ✅ Terrain (heightmap procedural)
- [x] `Mesh::CreateTerrain`: grade de fbm com normais por diferenças finitas,
  borda suave — determinístico pela semente
- [x] `TerrainComponent` (Segmentos/Tamanho/Elevação/Semente): mesh gerado sob
  demanda e usado no lugar do MeshRenderer; "Regenerar" no Inspetor
- [x] Serializado no `.kzscene` + menu Adicionar Componente

### ✅ Character Controller (cinemático)
- [x] `CharacterControllerComponent`: velocidade, gravidade, raio, altura,
  passo — movimento horizontal via `MoveCharacter` + chão por raycast 3D
- [x] C#: `Entity.AddCharacterController()` / `Entity.MoveCharacter(x, z)`;
  serializado + Inspetor + menu Adicionar Componente
- [x] v1 sem colisão com paredes (use Rigidbody/collider para isso)

### ✅ Timeline (cutscene / animação de transform)
- [x] `TimelineComponent`: keyframes de posição/rotação/escala interpolados
  linearmente, play/pause/loop/velocidade — roda no Play e no preview do editor
- [x] C#: `AddTimeline`, `PlayTimeline`, `SetTimelineTime`, `AddTimelineKeyframe`
- [x] Inspetor: transport (tocar/pausar/loop/velocidade/tempo) + lista de
  keyframes + "+ Keyframe (posição atual)"; serializado

### ✅ Culling por tamanho de tela
- [x] Além do frustum: objeto cuja AABB projetada cabe em ~1px não é
  desenhado (detalhe irrelevante àquela distância)

### ✅ Starter Pack (peso "de engine de verdade")
- [x] Content Pack EXPANDIDO nas Releases (ambas as plataformas): +modelos
  glTF (RiggedFigure, ReciprocatingSaw, Avocado, BoomBox, CesiumMilkTruck,
  SimpleMaterials) e +HDRIs (the_sky_is_on_fire, studio_small_03,
  spruit_sunrise, kloppenheim_06, 4k) — distribuição passa de dezenas de MB
  para centenas de MB
- [x] **Prefabs prontos** commitados em `content/Prefabs/`: Cubo,
  Personagem (cápsula + física), Moeda (emissiva) e Inimigo (física) — vão
  junto no release, arraste pro viewport
- [x] O peso "de engine" = engine + ferramentas (enxuto, C++ compilado) +
  Content Pack volumoso (CC0); o core continua leve, o pacote distribuído é
  que cresce como Unity/Unreal

---
## ✅ v0.28.1 — Céu, presets e correções de Release (patch)

- [x] **Demo 3D passa a achar o Content Pack** em `content/` e `../content/`
  (antes só `../content`) — no zip da Release o HDRI nunca carregava e o céu
  caía sempre no gradiente embutido
- [x] **Gradiente embutido regerado**: zênite azul, horizonte claro quente,
  chão suave (antes azul-escuro + chão quase preto)
- [x] **Botão "..." restaurado** no HDRI do céu (Project Settings → Gráficos):
  abre o diálogo nativo de arquivo; botão **Limpar** volta ao gradiente
- [x] **Presets High/Ultra sem DOF e motion blur por padrão** — o foco fixo
  borrava o viewport de câmera livre (e o Play parecia "voltar ao Medium")
- [x] **Bloom Ultra** com menos iterações (4 → 3): glow mais contido
- [x] Release v0.28.1 publicada (ambas as plataformas)

---
## ✅ v0.29 — Scripting de jogo (API C# + runtime)

- [x] **`Scene.EntitiesWithTag(tag)`**: todas as entidades com o Tag dado
  (filtro por Tag & Layers em runtime)
- [x] **Hierarquia em runtime**: `Entity.Parent`, `Entity.ChildCount`,
  `Entity.GetChild(index)` e `Entity.GetChild(nome)`
- [x] **`Entity.Active` (set/get)** — estilo GameObject.SetActive: entidade
  inativa NÃO é desenhada nem atualizada (render 3D/2D/UI, animações de
  sprite/esqueleto, timeline, character controller, física 2D/3D, áudio e
  scripts); filhos herdam o estado (ativa só se ela e todos os pais forem)
- [x] **`Entity.Position`** (getter local) — espelho do SetPosition
- [x] Inspetor: checkbox **"Ativo"** ao lado do nome
- [x] Serializado no `.kzscene` (e sobrevive a Play/snapshot/undo/prefab)

## ✅ v0.30 — Câmera de jogo, Input e Tempo

- [x] **`CameraFollowComponent`**: câmera segue um alvo pelo NOME com lerp
  exponencial (Suavidade), offset atrás do alvo, opção de **girar com o
  alvo** e **offset em espaço mundo** — roda no runtime (Play/GameView)
- [x] Serializado + Inspetor (**Camera Follow**) + menu Adicionar Componente
- [x] C#: `Entity.AddCameraFollow/SetCameraFollowTarget/SetCameraFollowOffset/
  SetCameraFollowSmoothness`
- [x] **`Input.IsMouseButtonDown`** — true só no frame do clique (edge)
- [x] **`Time.frameCount`** — contador de frames do runtime
- [x] **`Entity.AddTerrain`/`RegenerateTerrain`** — terreno procedural em runtime
- [x] C++ validado localmente (clang -fsyntax-only) e CI completo passou;
  releases **v0.29.0** e **v0.30.0** publicadas (ambas as plataformas)

---
## ✅ v0.31 — Mundo & Física (upgrade grande)

- [x] **Colisor de terreno (heightfield do Bullet)**: Rigidbody3D +
  TerrainComponent vira um `btHeightfieldTerrainShape` — personagens e
  objetos andam no relevo (a física respeita o heightmap)
- [x] **Character Controller v2**: `btKinematicCharacterController` (cápsula)
  — colide com PAREDES, sobe degraus (step), escorrega em rampas e aplica
  gravidade; fallback cinemático por raycast preservado
- [x] `Mesh::GetVertices()` — vértices em CPU (fonte das alturas do terreno)
- [x] **Física em runtime via C#**: `Scene.OverlapBox3D(center, half)`,
  `Scene.OverlapSphereAll3D(center, radius)` (TODAS as entidades na área)
- [x] **Demonstração Física 3D**: terreno heightfield + caixas/esferas
  dinâmicas caindo no relevo (menu Arquivo + toolbar); editor cria terreno
  com Rigidbody3D estático no menu Adicionar Componente
- [x] C++ validado (clang -fsyntax-only) + CI completo passou; **release
  v0.31.0 publicada** (ambas as plataformas)

---
## 🚧 v0.32 — Editor, física 2D e API (em andamento)

### Editor
- [x] **Gizmos de collider 3D com profundidade e rotação** (`Renderer3D::
  SubmitDebugLine`): wireframe abraça o objeto e é ocultado por geometria
  mais próxima — não flutua mais acima (era overlay de tela sem depth)
- [x] **Demonstração Física 2D**: dominós, rampa, pirâmide e bolas (Box2D)

### Física
- [x] **Rigidbody3D**: `GravityScale` (flutuação/invertida), `LinearDamping` e
  `AngularDamping` — Inspetor completo (Tipo/Massa/Gravidade/Damping) +
  serializado + C# (`SetGravityScale3D`/`SetDamping3D`)
- [x] **Rigidbody2D C#**: `ApplyForce`, `Get/SetAngularVelocity`,
  `SetFixedRotation`
- [x] **Tilemap em runtime com colliders**: `AddTilemap`/`SetTile`/
  `AddSolidTile` no C# + rebuild automático dos colliders Box2D
  (`CollidersDirty`)

### Scripting C#
- [x] **Partículas**: `SetParticleRate/Lifetime/Velocity/Gravity/Colors/
  Size/Additive`
- [x] **Animação de sprite**: `AddSpriteAnimation/PlaySpriteAnimation/
  SetSpriteAnimationFPS`
- [x] **Getters de transform**: `Entity.Rotation` e `Entity.Scale`
- [x] **Áudio por fonte**: `SetAudioVolume` / `SetAudioSpatial`
- [x] **`Scene.All`** — todas as entidades da cena; **`FindInChildren`** —
  busca recursiva por nome
- [x] **Movimento de jogo**: `GetForward/GetRight/MoveForward/MoveRight` e
  **`SetWorldPosition`** (converte pra local com pai)
- [x] **Exemplo jogável `DemoCharacter3D`** (SampleGame): terreno + character
  controller v2 + camera follow + gravidade alternada — validado pelo CI
  managed

---
---
## 🚀 v0.34 — Pilares AAA (em andamento)

Objetivo: transformar a engine de "jogos pequenos" pra "qualquer jogo" — os
seis pilares que separam um motor de jogo AAA de um motor de jogo indie.
Cada pilar é entregue funcional e utilizável (v1) e evolui nas versões
seguintes. Ordem de implementação = ordem de dependência.

### 1. IA e Navegação (NavMesh + agentes + máquina de estados)
- [x] **NavGrid** — grade de navegação 2D/3D (XZ) com A* + diagonal + suavização
  de caminho (simplificação por linha de visão na grade)
- [x] **NavObstacleComponent** — entidades que bloqueiam a grade (rasterização
  por AABB em runtime)
- [x] **NavAgentComponent** — segue caminho com suavização, rotação gradual e
  evitar obstáculos simples; API C++ (NativeScript) + C#
- [x] **EnemyAIComponent** — máquina de estados (Patrulha / Persegue / Ataca)
  com visão por raycast (linha de visão)
- [x] Debug visual: caminho + grade desenhados no viewport (DrawNavDebug)
- [x] **Demo IA**: jogador + 3 inimigos perseguindo com obstáculos (WASD)

### 2. Animação (blend trees + IK + camadas)
- [x] **BlendTreeComponent** (AnimationBlendComponent) — mistura de 2 clips por
  peso (idle↔walk pelo speed), TRS mix com slerp de rotação; C++ + C#
- [x] **TwoBoneIKComponent** — IK analítico de 2 ossos (braços/pernas) com
  peso e alvo em mundo; C++ + C#
- [ ] **Camadas** — blend por camada (corpo + braço apontando) (v1)
- [ ] **Retargeting** — mapeamento de ossos por nome pra skeletons diferentes (v1)

### 3. Renderização (culling + streaming + qualidade)
- [x] **Frustum culling** por entidade (AABB × frustum + culling por tamanho de tela) no passe 3D
- [x] **Culling de luzes** (pontual/spot fora do frustum+range não são submetidas)
- [ ] **Streaming de assets**: carregamento de texturas/malhas em thread
  (callback quando pronto), cache por prioridade
- [ ] **LOD automático** — geração de versões reduzidas de malhas procedurais
  (builtins) + integração com LODComponent
- [ ] **Oclusão** simples (portais/occluders por caixa) (v1)

### 4. Rede (multiplayer)
- [x] **Camada UDP confiável** — envio/recepção com ACK + retransmissão +
  numeração de pacotes (host/cliente, sem servidor dedicado na v1)
- [x] **Serialização de estado** — NetTransform (id+pos+yaw+flags) empacotado
  em 17 bytes (delta por estado inteiro)
- [x] **KZNetwork API** — Host/Connect/Send/PollEvent + eventos de conexão;
  C++ (kizuri::Network) + C# (Kizuri.Network)
- [ ] **Demo multiplayer local** — duas instâncias do Sandbox na mesma máquina (pendente)

### 5. Editor (ferramentas AAA)
- [ ] **Editor de partículas** (painel dedicado com preview)
- [ ] **Escultura de terreno** — pintar altura no TerrainComponent (regenera
  mesh + collider em tempo real)
- [ ] **Editor de animação** — preview de clips + curva de blend
- [ ] **Timeline de cutscene** — editor visual de keyframes
- [ ] **Editor de shaders** (visual, nós) (v1 — mais longe)

### 6. Pipeline de conteúdo (assets + física + áudio)
- [x] **Collider de malha convexa** (btConvexHullShape do Mesh::GetVertices) —
  MeshColliderComponent 3D (amostragem de pontos)
- [x] **Áudio com oclusão** — raycast fonte→ouvinte abafa o som (volume);
  reverb básico pendente
- [ ] **Streaming de cenas** — carregar cena grande em partes (stepwise) com
  progresso (o serializador já tem BeginDeserializeStepwise)
- [ ] **Async loader de texturas** (parte do pilar 3)

---
