# Roadmap — Kizuri Engine v1

> Objetivo da v1: uma engine **funcional e jogável**, sem ambição AAA, onde já
> dá para fazer jogos de verdade em 2D, 3D ou híbridos — e empacotá-los num
> executável standalone. A v2 (reescrita) está **cancelada**; todo o esforço
> fica na v1 legado.

---

## ✅ O que já está feito (base jogável)

### Render e gameplay
- [x] Renderer 3D PBR + IBL + CSM + Bloom + partículas
- [x] Renderer 2D: sprites, círculos, grid
- [x] Serialização 3D: MeshRenderer + material com caminhos (builtin/`.obj`/texturas)
- [x] Texto/HUD (`TextComponent` + `TextRenderer` com fonte embutida)
- [x] Tilemap 2D com atlas (colunas/linhas explícitas, UVs corretos)
- [x] Animação de sprite por frames (sprite sheets)
- [x] Picking 2D no viewport (sprite/círculo/texto)
- [x] Áudio por eventos (`Play/Stop/IsPlaying`) além de `PlayOnStart`
- [x] Física 2D (Box2D) e 3D (Bullet3)
- [x] Scripting C++ nativo via GameModule (`NativeScript`)

### Editor
- [x] Hierarquia, Inspetor, Console, Content Browser
- [x] Gizmos (mover/rotacionar/escalar), undo/redo
- [x] Play/Stop com cópia isolada da cena + edição bloqueada durante o Play
- [x] Drag & drop de assets do Content Browser para o viewport
- [x] Picking 3D (raycast) e 2D (ponto) no viewport
- [x] Modos de viewport 2D/3D, gizmo de câmera, diálogos nativos (Windows)

### Produto
- [x] `KizuriGame` — executável de jogo standalone (cena inicial + GameModule)
- [x] Cenas híbridas (2D + 3D na mesma cena)

---

## 🎯 Próximas etapas (por prioridade)

### Fase 1 — Polir o fluxo de criação (curto prazo)
- [ ] **Auto-switch de viewport** ao selecionar objetos: selecionar uma entidade
      3D em modo 2D muda para 3D (e vice-versa), ou gizmo 2D que manipula o
      plano correto do objeto 3D — decidir UX e implementar
- [ ] **Colisões de tilemap** — tiles marcáveis gerarem colliders Box2D no Play
      (níveis de platformer de verdade)
- [ ] **Texto multilinha** — suporte a `\n` no `TextRenderer` + alinhamento
      (centro/direita) para HUD e diálogos
- [ ] **Painel de material** no Inspetor (tiling, mapas, cores por campo — já há
      campos; falta polir a UI)
- [ ] **Pintor de tilemap no viewport** (desenhar/borracha sobre a grade) em vez
      de editar valores por campo
- [ ] **Preview de sprite/atlas no Inspetor** (thumbnail dos assets selecionados)

### Fase 2 — Gameplay completo (médio prazo)
- [ ] **Sistema de eventos de colisão** — callbacks `OnCollisionBegin/End` nos
      scripts (2D e 3D), a base para interações de verdade
- [ ] **Instanciação em runtime** — `Scene::Instantiate(prefab)` via script e
      por evento (tiros, inimigos, coletáveis)
- [ ] **Cenas transicionáveis** — carregar outra `.kzscene` a partir do script
      (telas de menu → jogo → game over)
- [ ] **Tilemap com múltiplas camadas** + tags de colisão por tile
- [ ] **AudioSource com trigger por colisão/evento** (ligar no sistema de eventos)
- [ ] **Camadas/ordenação 2D** (sorting layer + order in layer)

### Fase 3 — Produção (médio/longo prazo)
- [ ] **Build/export completo** — empacotar cena + GameModule + assets num
      distributível único (zip/instalador), com cena inicial configurável
- [ ] **Content Browser de verdade** — abrir projeto `.kzproj` mostra `Assets/`,
      importação por botão, renomear/apagar, busca
- [ ] **Pipeline de import** — normalização de `.obj`, geração de normais,
      previews em disco, reimport ao modificar
- [ ] **Animações 3D** (esqueletos/skinning) — não iniciado, grande
- [ ] **KZScript** — linguagem própria que gera código C++ compilável no fluxo
      atual (a API `NativeScript` já foi desenhada para isso)
- [ ] **Testes automatizados** — suíte de testes para serializer, física e
      componentes (hoje `KZ_BUILD_TESTS` existe mas não há testes)

---

## 🧹 Dívida técnica conhecida / limpeza

- [ ] **CSM** — blend suave entre cascatas e texel snapping (hoje pode haver
      corte visível/tremida sutil; cosmético)
- [ ] **Bloom** — limiar/intensidade fixos no shader; expor na UI
- [ ] **Diálogos nativos de arquivo só no Windows** — Linux/macOS usam campo de
      texto manual (funciona, sem diálogo)
- [ ] **`cmake/glad_stub/`** — código morto não referenciado; remover
- [ ] **Stats do Renderer2D** — círculos contam como quads nas estatísticas
- [ ] **Picking de texto** — bounds não cobrem descenders (parte inferior de
      letras como g/j/p pode não clicar)

---

## 📌 Como decidir prioridades

1. **Bloqueio de fluxo** → o que impede terminar um jogo do zero na engine
   (ex.: colisão de tilemap, instanciação)
2. **UX do editor** → o que torna o editor lento/estranho de usar
   (ex.: pintor de tilemap, auto-switch de viewport)
3. **Produção** → o que impede entregar um jogo final (export, dialogo de abrir projeto)
4. **Polimento** → cosmético, faz depois (CSM blend, stats, descenders)
