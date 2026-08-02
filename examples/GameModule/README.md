# GameModule — Template de scripts C++ (exemplo)

Biblioteca dinâmica (`GameModule.dll` / `libGameModule.so`) com scripts de
exemplo. Carregue no editor em **Arquivo > Carregar GameModule** — as classes
aparecem no dropdown **Script Nativo** do Inspetor.

---

## Compilar

```bash
# Junto com a engine (recomendado)
cmake -B build && cmake --build build --target GameModule
# Saída: build/bin/GameModule.dll (ou libGameModule.so)
```

Standalone (projeto real):

```bash
cmake -B build -DKIZURI_ENGINE_DIR=/caminho/pra/Kizuri-Engine-main
cmake --build build
```

Use o **mesmo compilador** que compilou o editor.

---

## Scripts de exemplo

| Script | Demonstra |
|---|---|
| **RotatorScript** | `OnUpdate` + `Transform` |
| **PlayerController** | Input WASD; com `Rigidbody2D` Dynamic usa `SetLinearVelocity` |
| **BouncerScript** | `ApplyLinearImpulse` (espaço = pulo) |
| **CollectibleScript** | `OnCollisionBegin` + `DestroyEntity()` |

## API útil em scripts

```cpp
GetComponent<T>() / GetEntity() / GetScene()
Instantiate("Assets/bullet.kzprefab", pos)  // spawn
LoadScene("Assets/Level2.kzscene")          // troca diferida
DestroyEntity()
OnCollisionBegin/End(Entity other)

Rigidbody2DComponent::SetLinearVelocity / ApplyLinearImpulse / SetTransform
AudioSourceComponent::Play() / Stop()
```

Fluxo completo: editar cena → Play → **Arquivo > Exportar Jogo...** → pasta com `KizuriGame` + `Start.kzscene` + assets.
