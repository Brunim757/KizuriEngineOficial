# Roadmap — Kizuri Engine

> Objetivo final: motor C++ 100% privado; o jogo é um assembly **C#**
> (`Kizuri.Scripting.dll`), conversando com a engine só por um ABI C. Nada
> de headers/libs internos expostos. (Antes era "GameModule em C++".)

> Objetivo da v1: engine **funcional e jogável** — criar no editor, scriptar
> em C# (assembly managed) e exportar um executável standalone.

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

### Editor / produto
- [x] Hierarquia, Inspetor, Console, Content Browser, gizmos, Play/Stop
- [x] Salvar Prefab (menu de contexto) + arrastar `.kzprefab` pro viewport
- [x] **Exportar Jogo...** (pasta com KizuriGame + cena + assets + módulo)
- [x] Cena inicial do projeto (`.kzproj` → `StartScenePath`)
- [x] Caminhos de asset relativos ao projeto
- [x] `KizuriGame` com resize de viewport e troca de cena

---

## 🎯 Próximas etapas

### Polimento de fluxo
- [ ] Texto alinhamento avançado / dialogos no editor
- [ ] Preview de sprite/atlas no Inspetor
- [ ] Painel de material mais completo
- [ ] Camadas/ordenação 2D (sorting layer)

### Produção
- [ ] Pipeline de import (normais, reimport, previews)
- [ ] Animações 3D (skinning)
- [x] Runtime C# na engine (host CoreCLR via hostfxr carrega `Kizuri.Scripting`)
- [ ] Testes automatizados (`KZ_BUILD_TESTS`)

---

## 🧹 Dívida técnica

- [ ] CSM blend / texel snapping
- [ ] Bloom exponível na UI
- [ ] Diálogos nativos de arquivo só no Windows
- [ ] Remover `cmake/glad_stub/`
- [ ] Stats Renderer2D (círculos) / picking de texto (descenders)
