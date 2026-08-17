# Roadmap AAA — o que falta para um jogo no padrão GTA 4

> Lista de trabalho (não promessa): o que a Kizuri Engine ainda precisa
> para sustentar um projeto grande de mundo aberto com veículos, IA,
> narrativa e UI de console. Organizada por pilar; cada item = feature.

## 1. Mundo aberto / streaming
- [ ] Chunked world: dividir a cena em chunks carregados/descarregados por distância
- [ ] Terreno de altura (heightmap) com LOD por distancia (não só cubos)
- [ ] Vegetação/objetos instanciados (GPU instancing já existe p/ partículas — expandir)
- [ ] Navmesh do mundo inteiro com streaming (hoje o NavGrid é por cena)
- [ ] Céu dinâmico com ciclo dia/noite (hoje é procedural estático)
- [ ] Clima: chuva, neblina volumétrica, vento
- [ ] Water rendering (planície de água com reflexos/refracção)
- [ ] Posto de carregamento: Save do mundo em chunks

## 2. Jogabilidade
- [ ] Veículos: carros/motos com física de chassis (Bullet vehicle), volante, câmera de perseguição
- [ ] Pessoas pedrestres (NPCs animados com Fox-style skeleton) — precisa de sistema de navmesh + wander AI
- [ ] Interações: prompts "pressione E" (UI de prompt já existe parcialmente via UIButton)
- [ ] Inventário/armas: slots, troca, recarga, dano por parte do corpo
- [ ] Sistema de missões: quests com etapas, objetivos no minimapa, checkpoints
- [ ] Economia: dinheiro, lojas, compra/venda
- [ ] HUD estilo console: minimapa, rádio (lista de músicas), barra de saúde/armadura

## 3. IA
- [ ] Estado de IA (idle/patrulha/perseguir/combate) — hoje só EnemyAI básico
- [ ] Busca de caminho com animação + evasão de obstáculos (steering)
- [ ] Detecção: linha de visão, audição, alcance
- [ ] Grupos: gangues seguem líder, reações em conjunto
- [ ] Polícia/foragido: estrelas de procurado, spawn de viaturas

## 4. Narrativa
- [ ] Sistema de diálogo (falas, escolhas, árvore) com legendas + dublagem
- [ ] Cutscenes autoráveis no editor (Timeline existe — faltam câmeras de corte, atores, legendas)
- [ ] Sistema de rádio/músicas licenciadas com playlist

## 5. Áudio (padrão AAA)
- [ ] Mixer: grupos (música/efeitos/voz) com volumes e ducking
- [ ] Fade in/out de músicas; rádio com estações
- [ ] Reverb por zona (hoje só reverb global)
- [ ] Audio occlusion (paredes abafam som)
- [ ] Dublagem por linha de diálogo

## 6. Renderer/qualidade
- [ ] Vegetação com sombra própria + wind
- [ ] Diferimentos de qualidade: escala de resolução dinâmica, VRS (se suportado)
- [ ] Reflexos por plano/SSR estáveis + reflexos em vidro de carro
- [ ] HDR em janelas/neon (emissivos já existem)
- [ ] Motion blur/câmera de explosão já existem — falta film grain/vignete por presets
- [ ] Performance: oclusão por GPU (hoje é CPU por occluders), frustum culling por chunk
- [ ] Árvore de decisão de LOD automática (LODComponent existe — falta gerar LODs)

## 7. UI/UX de jogo
- [ ] Menu principal: novo jogo, continuar, configurações, créditos
- [ ] Pausa com menu embutido
- [ ] Telas de carregamento com dicas
- [ ] Mapas grandes com zoom (UI de minimapa com textura)
- [ ] Suporte a gamepad/controle (inputs analógicos + vibração)
- [ ] Localização: strings externas (pt/en/es), fontes com acentos completos

## 8. Ferramentas/editor
- [ ] Sistema de prefabs com herança (kzprefab existe — falta instância vinculada)
- [ ] Asset pipeline: importar glTF com animações compiladas, atlas automático
- [ ] Terrain editor no viewport (paint de altura/textura)
- [ ] Timeline de cutscene com curvas de câmera
- [ ] Profiler por sistema (CPU/GPU por pass)
- [ ] Exporter de cenas grandes com streaming
- [ ] Play from node (entrar no Play a partir de uma entidade/ponto)

## 9. Plataforma
- [ ] Gamepad/teclado remapeável persistido
- [ ] Achievements + save cloud (via API do site)
- [ ] Telemetria anônima (eventos de gameplay)
- [ ] Anti-aliasing por MSAA (existe) + TAA estável em movimento (existe, precisa tuning)
- [ ] FSR/DLSS-style upscale quando disponível
- [ ] Vsync adaptativo, limite de FPS, presets gráficos por dispositivo
- [ ] Multiplayer: hoje só rede básica (socket) — faltam autoridade de servidor, lag compensation, matchmaking

## 10. Netcode (para o online do GTA-style)
- [ ] Arquitetura cliente-servidor com autoridade
- [ ] Interpolação/extrapolação de posição, reconciliação
- [ ] Salas/lobby, heartbeat, reconexão
- [ ] Sincronização de veículos (estado do chassis), NPCs, missões
