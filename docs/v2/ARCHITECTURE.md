# Kizuri Engine v2 Architecture

## PT-BR

### Camadas principais

1. `Foundation`
   Base compartilhada com memoria, jobs, profiling, arquivos, serializacao, logging, UUIDs e reflection leve.
2. `Platform`
   Janela, input, threads, gamepad, monitor, filesystem e servicos de sistema operacional.
3. `RHI`
   Camada de abstracao de render com foco em compatibilidade ampla e evolucao de backend.
4. `Renderer`
   Frame graph, materiais, shader pipeline, iluminacao, sombras, pos-processo, `2D`, `3D` e UI runtime.
5. `World`
   ECS, entidades, componentes, prefabs, cenas, streaming e simulacao.
6. `Asset Pipeline`
   Importacao, conversao, cache, cooking, versionamento e dependencias.
7. `Scripting`
   Host `CoreCLR`, assemblies `C#`, bridge native-managed e tooling de script.
8. `Editor`
   Projeto, viewport, hierarchy, inspector, graph tools, profiler e build/export.

### Modulos planejados

- `KizuriV2Foundation`
- `KizuriV2Platform`
- `KizuriV2Asset`
- `KizuriV2RHI`
- `KizuriV2Renderer`
- `KizuriV2World`
- `KizuriV2Physics2D`
- `KizuriV2Physics3D`
- `KizuriV2Audio`
- `KizuriV2Scripting`
- `KizuriV2Editor`

### Regras arquiteturais

- `Editor` nunca pode concentrar toda a logica do produto em um unico arquivo monolitico.
- `World` nao pode ser dono direto de renderer, audio, fisica e scripting no mesmo bloco de codigo.
- Todo asset de runtime precisa ter `AssetId`, metadados, dependencia e formato cozido.
- Nenhum sistema critico deve depender de estado global irrestrito.
- Sistemas pesados devem expor configuracoes de qualidade para `Low`, `Medium`, `High` e `Ultra`.

### Direcao de render

- Arquitetura preparada para multiplos backends.
- Base de compatibilidade ampla, com qualidade escalavel por perfil de hardware.
- Pipeline inicial recomendada: `Forward+` ou `Clustered Forward`.
- Recursos-alvo: `PBR`, sombras, SSAO, bloom, volumetria seletiva, particulas, decals e terreno.
- Recursos posteriores: `GI` avancada, compute-heavy VFX, ferramentas de cinematica e features enterprise.

### Mundos e streaming

- `World Partition` e streaming entram na fundacao.
- Cenas grandes devem ser divididas em regioes/tiles/chunks com carregamento assincrono.
- O editor precisa enxergar a diferenca entre dado autoral, dado cozido e dado em memoria.
- O runtime precisa suportar carregamento incremental sem travar o frame principal.

### Scripting

- `C++` continua sendo o nucleo de performance e integracao profunda.
- `C#` vira a camada padrao de gameplay e automacao de alto nivel.
- O bridge entre nativo e gerenciado precisa ser estavel e orientado por API clara, nao por ABI fragil.

---

## EN

### Main layers

1. `Foundation`
   Shared base for memory, jobs, profiling, files, serialization, logging, UUIDs, and lightweight reflection.
2. `Platform`
   Windowing, input, threads, gamepad, monitor, filesystem, and OS services.
3. `RHI`
   Render abstraction layer focused on broad compatibility and backend evolution.
4. `Renderer`
   Frame graph, materials, shader pipeline, lighting, shadows, post-processing, `2D`, `3D`, and runtime UI.
5. `World`
   ECS, entities, components, prefabs, scenes, streaming, and simulation.
6. `Asset Pipeline`
   Import, conversion, cache, cooking, versioning, and dependency tracking.
7. `Scripting`
   `CoreCLR` host, `C#` assemblies, native-managed bridge, and script tooling.
8. `Editor`
   Project system, viewport, hierarchy, inspector, graph tools, profiler, and build/export.

### Planned modules

- `KizuriV2Foundation`
- `KizuriV2Platform`
- `KizuriV2Asset`
- `KizuriV2RHI`
- `KizuriV2Renderer`
- `KizuriV2World`
- `KizuriV2Physics2D`
- `KizuriV2Physics3D`
- `KizuriV2Audio`
- `KizuriV2Scripting`
- `KizuriV2Editor`

### Architectural rules

- `Editor` must never centralize the whole product logic in a single monolithic file.
- `World` cannot directly own renderer, audio, physics, and scripting as one large code block.
- Every runtime asset must have an `AssetId`, metadata, dependency info, and cooked format.
- No critical system should depend on unrestricted global state.
- Heavy systems must expose quality profiles for `Low`, `Medium`, `High`, and `Ultra`.

### Rendering direction

- Architecture prepared for multiple backends.
- Broad compatibility base with scalable quality per hardware profile.
- Recommended initial pipeline: `Forward+` or `Clustered Forward`.
- Target features: `PBR`, shadows, SSAO, bloom, selective volumetrics, particles, decals, and terrain.
- Later features: advanced `GI`, compute-heavy VFX, cinematics tooling, and enterprise-grade features.

### Worlds and streaming

- `World Partition` and streaming belong in the foundation.
- Large scenes should be split into regions/tiles/chunks with asynchronous loading.
- The editor must distinguish authored data, cooked data, and runtime memory data.
- The runtime must support incremental loading without stalling the main frame.

### Scripting

- `C++` remains the core for performance and deep integration.
- `C#` becomes the default layer for gameplay and high-level automation.
- The native-managed bridge must be stable and API-driven, not based on fragile ABI coupling.
