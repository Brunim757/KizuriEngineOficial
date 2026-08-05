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
