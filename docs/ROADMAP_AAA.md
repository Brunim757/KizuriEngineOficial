# Roadmap AAA — capacidade de um jogo grande

> **Atualizado para v0.41.0** — todos os itens abaixo agora estão no
> `ROADMAP.md` principal como backlog v0.41+. Esta lista é espelho
> histórico com status de implementação.

## 1. Mundo aberto / streaming
- [x] Chunked world: dividir a cena em chunks carregados/descarregados por distância
- [ ] Terreno de altura (heightmap) com LOD por distancia (não só cubos)
- [ ] Vegetação/objetos instanciados (GPU instancing já existe p/ partículas — expandir)
- [ ] Navmesh do mundo inteiro com streaming (hoje o NavGrid é por cena)
- [ ] Céu dinâmico com ciclo dia/noite (hoje é procedural estático)
- [ ] Clima: chuva, neblina volumétrica, vento
- [ ] Water rendering (planície de água com reflexos/refracção)
- [ ] Posto de carregamento: Save do mundo em chunks

## 2. Jogabilidade
- [ ] 🌍 Veículos: carros/motos com física de chassis (Bullet vehicle), volante, câmera de perseguição
- [ ] 🌍 Pessoas pedestres (NPCs animados com Fox-style skeleton) — precisa de navmesh + wander AI
- [ ] Interações: prompts "pressione E" (UI de prompt já existe parcialmente via UIButton)
- [ ] Inventário/armas: slots, troca, recarga, dano por parte do corpo
- [ ] Sistema de missões: quests com etapas, objetivos no minimapa, checkpoints
- [ ] Economia: dinheiro, lojas, compra/venda
- [ ] HUD estilo console: minimapa, rádio (lista de músicas), barra de saúde/armadura

## 3. IA
- [x] Estado de IA (idle/patrulha/perseguir/combate) — EnemyAIComponent implementa os 3 estados
- [x] Detecção: linha de visão por raycast no EnemyAI
- [ ] Busca de caminho com animação + evasão de obstáculos (steering)
- [ ] Detecção: audição, alcance
- [ ] Grupos: gangues seguem líder, reações em conjunto
- [ ] 🌍 Polícia/foragido: estrelas de procurado, spawn de viaturas

## 4. Narrativa
- [ ] Sistema de diálogo (falas, escolhas, árvore) com legendas + dublagem
- [x] Cutscenes autoráveis — TimelineComponent existe com keyframes TRS
- [ ] Sistema de rádio/músicas licenciadas com playlist

## 5. Áudio (padrão AAA)
- [ ] Fade in/out de músicas; rádio com estações
- [x] Reverb — flag `Reverb` no AudioSourceComponent
- [ ] Dublagem por linha de diálogo

## 6. Renderer/qualidade
- [ ] Vegetação com sombra própria + wind
- [ ] Diferimentos de qualidade: escala de resolução dinâmica, VRS
- [x] Reflexos SSR 3.3-safe + reflexões planares (espelho real)
- [x] HDR em janelas/neon (emissivos + emissive map)
- [x] Film grain/vignete no pós-processamento
- [ ] Performance: oclusão por GPU (hoje é CPU por occluders)
- [x] LOD automática — LODComponent + CreateLODMesh + botão "Gerar LOD automático"

## 7. UI/UX de jogo
- [ ] Menu principal: novo jogo, continuar, configurações, créditos
- [ ] Pausa com menu embutido
- [ ] Telas de carregamento com dicas
- [ ] Mapas grandes com zoom (UI de minimapa com textura)
- [ ] Suporte a gamepad/controle (inputs analógicos + vibração)
- [ ] Localização: strings externas (pt/en/es), fontes com acentos completos

## 8. Ferramentas/editor
- [ ] Sistema de prefabs com herança (kzprefab existe — falta instância vinculada)
- [x] Asset pipeline: importar glTF com animações compiladas (cgltf)
- [x] Terrain editor no viewport — pincel de escultura (levanta/afunda)
- [ ] Timeline de cutscene com curvas de câmera
- [ ] Profiler por sistema (CPU/GPU por pass)
- [ ] Exporter de cenas grandes com streaming
- [ ] Play from node (entrar no Play a partir de uma entidade/ponto)

## 9. Plataforma
- [ ] Gamepad/teclado remapeável persistido
- [ ] Achievements + save cloud (via API do site)
- [ ] Telemetria anônima (eventos de gameplay)
- [ ] FSR/DLSS-style upscale quando disponível
- [ ] Vsync adaptativo, limite de FPS, presets gráficos por dispositivo
- [ ] Multiplayer: autoridade de servidor, lag compensation, matchmaking

## 10. Netcode
- [ ] Arquitetura cliente-servidor com autoridade
- [ ] Interpolação/extrapolação de posição, reconciliação
- [ ] Salas/lobby, heartbeat, reconexão
- [ ] Sincronização de veículos (estado do chassis), NPCs, missões
