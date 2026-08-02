# GameModule — Template de scripts C++ (exemplo)

Este diretório é um **GameModule pronto pra usar**: uma biblioteca dinâmica
(`GameModule.dll` no Windows, `libGameModule.so` no Linux) com scripts de
exemplo que o editor carrega em **Arquivo > Carregar GameModule** e que
aparecem no dropdown **Script Nativo** do Inspetor.

É o mesmo formato de código que o editor gera em `Source/` quando você cria
um projeto novo (menu **Projeto > Novo Projeto**) — este diretório é uma
versão maior, com 3 scripts de exemplo comentados pra aprender o caminho.

---

## Como compilar

### Opção 1 — junto com a engine (recomendado)

O build raiz já inclui este diretório (`KZ_BUILD_GAMEMODULE`, ligado por
padrão). Depois de configurar o build da engine normalmente, o `GameModule`
é compilado junto e o binário cai na mesma pasta `bin/` do editor:

```bash
cmake -B build
cmake --build build --target GameModule
# ou, se já compilou tudo: cmake --build build
```

O resultado fica em `build/bin/GameModule.dll` (ou `libGameModule.so`),
**ao lado do editor e da KizuriEngine.dll** — que é exatamente onde ele
precisa estar pra ser carregado.

### Opção 2 — standalone (o mesmo fluxo de um projeto real)

Copie esta pasta para onde quiser (ex.: dentro do `Source/` de um projeto
criado pelo editor) e aponte pro checkout da engine:

```bash
cmake -B build -DKIZURI_ENGINE_DIR=/caminho/pra/Kizuri-Engine-main
cmake --build build
```

> ⚠️ **Compilador:** use o **mesmo** compilador (e a mesma versão) que
> compilou o editor. O módulo linka a `KizuriEngine` e o sistema operacional
> reaproveita a cópia já carregada no processo — se os símbolos/layout de
> struct não baterem, pode travar ou dar comportamento indefinido.

---

## Como carregar e testar

1. Compile (opção 1 ou 2 acima).
2. Abra o editor e use **Arquivo > Carregar GameModule...**.
3. No campo de caminho, aponte pro arquivo gerado:
   - Windows: `.../build/bin/GameModule.dll`
   - Linux: `.../build/bin/libGameModule.so`
4. O console mostra: *"Módulo de jogo carregado: ... (3 scripts registrados)"*.
5. Selecione uma entidade e adicione um script em **Adicionar Componente >
   Script Nativo** — escolha uma das 3 classes no dropdown.
6. Aperte **Play**. O console do editor mostra os logs dos scripts.

---

## Os scripts de exemplo

| Script | O que demonstra | Como testar |
|---|---|---|
| **RotatorScript** | `OnUpdate`, `TransformComponent`, `Timestep` | Crie um cubo 3D (com MeshRenderer), adicione o script e dê Play — o cubo gira |
| **PlayerController** | `OnCreate`, `Input::IsKeyPressed`, movimento | Crie um sprite 2D (arraste um .png pro viewport), adicione o script e dê Play — WASD/setas movem |
| **BouncerScript** | Física 2D (`RuntimeBody`/Box2D) | Crie um sprite com **Rigidbody2D (Dinâmico)** + **Box Collider 2D** + o script; crie um chão (sprite com Rigidbody2D Estático + colisor) embaixo; dê Play e segure **ESPAÇO** |

---

## Criando seu próprio script

1. Crie `MeuScript.hpp/.cpp` nesta pasta (`src/`), herdando de
   `kizuri::NativeScript` e sobrescrevendo `OnCreate()` / `OnUpdate(Timestep)`
   / `OnDestroy()` conforme precisar.
2. Registre no `RegisterScripts()` do `src/GameModule.cpp`:
   ```cpp
   registry.Register<MeuScript>("MeuScript");
   ```
3. Recompile (`cmake --build build`) e recarregue o módulo no editor
   (**Arquivo > Carregar GameModule...** de novo) — o script aparece no
   dropdown do Inspetor.

Referência rápida da API disponível dentro de um script (tudo via
`#include <Kizuri.hpp>`):

- `GetComponent<T>()` / `GetEntity()` — componentes e entidade
- `TransformComponent` (`Translation`, `Rotation`, `Scale`)
- `Input::IsKeyPressed(Key::A)` — teclado (ver `Key::` em `Input.hpp`)
- `KZ_INFO/KZ_WARN/KZ_ERROR("texto {0}", valor)` — console do editor
- Física: `Rigidbody2DComponent::ApplyLinearImpulse(...)` (o `RuntimeBody` também está exposto, mas o Box2D é interno da engine)
