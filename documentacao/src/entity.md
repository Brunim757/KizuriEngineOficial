---
title: Entity — referência
group: Scripting C#
order: 3
---

# Entity — referência

`Entity` é um handle opaco para uma entidade da cena. A engine é fechada —
você nunca vê o EnTT por baixo.

## Propriedades

| Membro | Tipo | Descrição |
|--------|------|-----------|
| `Entity.Invalid` | `Entity` | Handle inválido (não existe) |
| `IsValid` | `bool` | `Handle != 0` |
| `Id` | `uint` | Identificador numérico |
| `Name` | `string` | Nome (Tag) — **leitura e renomeio em runtime** |

```csharp
Entity e = Scene.CreateEntity("Jogador");
e.Name = "Herói";
Log.Info($"Id={e.Id}, nome={e.Name}, válida={e.IsValid}");
```

## Checagem de componentes

| Membro | Retorna |
|--------|---------|
| `HasComponent(ComponentType t)` | `bool` — tem o componente? |
| `HasTransform` / `HasRigidbody2D` / `HasSprite` / `HasText` / `HasAudio` / `HasCamera` | atalhos |

## Transform

| Método | Descrição |
|--------|-----------|
| `TryGetTransform(out Transform t)` | Transform local (Translation, Rotation em **rad**, Scale) |
| `SetPosition(Vector3)` | Posição local |
| `SetRotation(Vector3)` | Rotação local, euler em radianos |
| `SetScale(Vector3)` | Escala local |
| `TryGetWorldPosition(out Vector3)` | **Posição mundial** (respeita hierarquia) |
| `LookAt(Vector3 target)` | Faz a entidade encarar um ponto |
| `SetParent(Entity)` / `SetParent()` | Parenta / destaca |

## Ciclo de vida e hierarquia

| Método | Descrição |
|--------|-----------|
| `Destroy()` | Destrói a entidade |
| `SetParent(Entity parent)` | Parenta a `parent` |
| `SetParent()` | Destaca da hierarquia |

## Adicionar componentes em runtime

| Método | Descrição |
|--------|-----------|
| `AddSprite(string? texture = null)` | Sprite 2D (sem textura = cor sólida) |
| `AddText(string text, float size = 48)` | Texto 2D |
| `AddAudio(string clip, bool loop, bool playOnStart)` | Áudio posicional |
| `AddCamera(bool perspective3D = false)` | Câmera |
| `AddLight(LightType, …)` | Luz (Direcional/Ponto/Spot) |
| `AddMeshRenderer(string meshSource)` | Mesh 3D |
| `AddAnimator(string meshPath)` | Animação esquelética |
| `AddRigidbody3D(BodyType3D, float mass)` | Corpo 3D |
| `AddBoxCollider3D(hx, hy, hz)` | Collider de caixa 3D |
| `AddSphereCollider3D(radius)` | Collider de esfera 3D |
| `AddCircleCollider2D(radius, density, friction, restitution)` | Collider circular 2D |
| `AddUICanvas(orthoSize)` | Canvas de UI |
| `AddUIRect(x, y, w, h, r, g, b, a)` | Retângulo de UI |
| `AddUIButton(x, y, w, h, r, g, b, a)` | Botão de UI |
| `AddUIText(text, size, r, g, b, a)` | Texto de UI |

## Mutação em runtime

| Método | Descrição |
|--------|-----------|
| `SetSpriteTexture(string)` | Troca a textura do sprite |
| `SetSpriteColor(r, g, b, a)` | Cor do sprite |
| `SetSpriteFlip(bool flipX, bool flipY)` | Inverte o sprite |
| `SetText(string)` / `SetTextSize(float)` / `SetTextColor(r, g, b, a)` | Texto |
| `SetMaterial(r, g, b, metallic, roughness)` | Material PBR |
| `SetMaterialAlbedoMap(path)` / `SetMaterialNormalMap(path)` / `SetMaterialMetallicRoughnessMap(path)` | Mapas |
| `SetMaterialEmissive(r, g, b, strength)` | Emissão (bloom) |
| `SetLightColor(r, g, b)` / `SetLightIntensity(float)` | Luz |
| `SetGravityScale(float)` | Gravidade do corpo 2D |
| `SetSortingLayer(int)` | Ordenação 2D |
| `PlayAudio()` / `StopAudio()` | Áudio do source |
| `SetCamera(fovDeg, near, far)` | Parâmetros de câmera |
| `SetParticleTexture(string)` | Textura das partículas |
| `UIButtonWasClicked()` / `UIButtonIsHovered()` | Estado do botão |
| `SetUIRect(x, y, w, h)` / `SetUIColor(r, g, b, a)` | UI |

## Animação esquelética

| Método | Descrição |
|--------|-----------|
| `PlayAnimation(string clipName)` | Toca um clip da skin (retorna `true` se achou) |
| `AnimationTime` / `SetAnimationTime(float)` | Posição (segundos) |
| `SetAnimationSpeed(float)` | Velocidade |
| `SetAnimationLoop(bool)` | Loop |
| `SetAnimationPlaying(bool)` | Toca/pausa |

## Física 3D

| Método | Descrição |
|--------|-----------|
| `ApplyForce(Vector3)` | Força contínua |
| `ApplyImpulse(Vector3)` | Impulso instantâneo |
| `ApplyTorque(Vector3)` | Torque |
| `SetVelocity(Vector3)` / `TryGetVelocity(out Vector3)` | Velocidade linear |
| `SetAngularVelocity(Vector3)` / `TryGetAngularVelocity(out Vector3)` | Velocidade angular |

## Enums

```csharp
enum ComponentType { Transform, Rigidbody2D, Sprite, Text, Audio, Camera,
                     Light, UIRect, UIButton, UICanvas, CircleCollider2D,
                     MeshRenderer, ParticleSystem, Animator, Rigidbody3D,
                     BoxCollider3D, SphereCollider3D }

enum BodyType3D    { Static = 0, Dynamic, Kinematic }
enum LightType     { Directional = 0, Point, Spot }
```

## Estrutura Transform

```csharp
public struct Transform
{
    public Vector3 Translation;
    public Vector3 Rotation; // euler, radianos
    public Vector3 Scale;
}
```

::: info
**Rotação em radianos.** A API C# usa sempre radianos (o Inspetor mostra em
graus). Converta com `graus * MathF.PI / 180f`.
:::
