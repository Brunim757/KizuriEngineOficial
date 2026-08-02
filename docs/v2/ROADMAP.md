# Kizuri Engine v2 Roadmap

## PT-BR

### Fase 0 - Fundacao

- Criar estrutura nova da `v2` ao lado da `v1-legado`.
- Estabelecer naming, modulos, padrao de build e convencoes tecnicas.
- Publicar documentacao fundadora bilíngue.

### Fase 1 - Core tecnico

- `Foundation`, `Platform` e `AssetId` unificado.
- Job system, logging, profiling e serializacao base.
- Formato inicial de projeto e manifesto de build.

### Fase 2 - Runtime renderizavel

- `RHI` inicial e renderer base.
- Janelas, swapchain/contexto, comandos de frame e camera.
- Primeiro caminho `2D` e `3D` funcional com materiais basicos.

### Fase 3 - Mundo e assets

- ECS da `v2`, entidades, prefabs e cenas.
- Importacao inicial de texturas, meshes e materiais.
- Cooking de assets e cache local.

### Fase 4 - Scripting `C#`

- Host `CoreCLR`.
- Compilacao de assemblies do jogo.
- API inicial para entidades, transform, camera, audio e input.
- Hot reload controlado em ambiente de editor.

### Fase 5 - Editor premium inicial

- Project browser.
- Hierarchy, inspector, content browser e viewport.
- Play/Stop, console, profiler basico e importacao por UI.

### Fase 6 - Sistemas de jogo

- Fisica `2D` e `3D`.
- Audio runtime.
- Particulas, animacao e materiais avancados.
- Primeira camada real de streaming/world partition.

### Fase 7 - Build e distribuicao

- Pipeline de export.
- Cooking por target.
- Empacotamento de jogo standalone.
- Workflow de `GitHub` para build oficial e publicacao de artefatos.

### Fase 8 - Release de respeito

- Estabilidade, otimização, telemetria de performance e testes direcionados.
- Polimento de UX do editor.
- Kits de documentacao publica e exemplos oficiais.
- Demo interna forte validando a stack `2D + 3D`.

### Regras de execucao

- A equipe inicial e pequena, entao a fundacao precisa ser ambiciosa e o escopo por milestone precisa ser controlado.
- Features de marketing entram so quando a base tecnica de runtime, tooling e assets estiver estavel.
- Cada fase deve deixar algo utilizavel, nao apenas infraestrutura invisivel.

---

## EN

### Phase 0 - Foundation

- Create the new `v2` structure alongside `v1-legacy`.
- Establish naming, modules, build standards, and technical conventions.
- Publish bilingual founding documentation.

### Phase 1 - Technical core

- `Foundation`, `Platform`, and unified `AssetId`.
- Job system, logging, profiling, and base serialization.
- Initial project format and build manifest.

### Phase 2 - Renderable runtime

- Initial `RHI` and base renderer.
- Windows, swapchain/context, frame commands, and camera.
- First functional `2D` and `3D` path with basic materials.

### Phase 3 - World and assets

- `v2` ECS, entities, prefabs, and scenes.
- Initial import for textures, meshes, and materials.
- Asset cooking and local cache.

### Phase 4 - `C#` scripting

- `CoreCLR` host.
- Game assembly compilation.
- Initial API for entities, transform, camera, audio, and input.
- Controlled hot reload in editor workflows.

### Phase 5 - Premium editor baseline

- Project browser.
- Hierarchy, inspector, content browser, and viewport.
- Play/Stop, console, basic profiler, and UI-driven import flow.

### Phase 6 - Gameplay systems

- `2D` and `3D` physics.
- Runtime audio.
- Particles, animation, and advanced materials.
- First real streaming/world partition layer.

### Phase 7 - Build and distribution

- Export pipeline.
- Per-target cooking.
- Standalone game packaging.
- `GitHub` workflow for official builds and artifact publishing.

### Phase 8 - Serious release

- Stability, optimization, performance telemetry, and focused tests.
- Editor UX polish.
- Public documentation kits and official examples.
- Strong internal demo validating the `2D + 3D` stack.

### Execution rules

- The initial team is small, so the foundation must be ambitious while each milestone remains tightly controlled.
- Marketing features only enter after runtime, tooling, and asset foundations are stable.
- Every phase must leave something usable, not only invisible infrastructure.
