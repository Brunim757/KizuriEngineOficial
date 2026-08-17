---
title: Entity (API)
group: Scripting C#
order: 2
---

# Entity — API

A **Entity** é qualquer objeto da cena: personagem, inimigo, câmera, luz,
UI. No script, `Entity` é a entidade que tem o script anexado.

## Propriedades

```csharp
Entity.Name;                    // nome (get/set)
Entity.Position;                // posição LOCAL (get/set)
Entity.Active;                  // ativa/inativa (bool, get/set)
Entity.Parent;                  // entidade pai (ou Invalid se raiz)
Entity.ChildCount;              // quantos filhos diretos
Entity.GetChild(0);             // filho no índice
Entity.GetChild("Olho");        // filho pelo nome
Entity.GetEulerAngles();        // rotação em graus (Vector3)
Entity.Scale;                   // Vector3
Entity.TryGetWorldPosition();   // posição de mundo (respeita o pai)
```

::: nota Entidade inativa (`Active = false`)
Não é desenhada nem atualizada: animações, timeline, física, áudio e scripts
ficam pausados, e os filhos herdam o estado (ativa só se ela e TODOS os pais
forem). Igual ao `SetActive` do GameObject.
:::

## Transform

```csharp
Entity.Move(x, y, z);                  // translada (espaço local)
Entity.SetPosition(x, y, z);
Entity.SetWorldPosition(x, y, z);      // posição de mundo (mesmo com pai)
Entity.Rotate(0f, 45f, 0f);
Entity.SetEulerAngles(x, y, z);
Entity.SetScale(x, y, z);
Entity.LookAt(outro);
Entity.GetForward();                 // direção pra onde olha (Vector3)
Entity.GetRight();                   // direção lateral
Entity.MoveForward(distancia);       // move ao longo da própria frente
Entity.MoveRight(distancia);         // move pro lado
```

## Criar conteúdo

```csharp
Entity.AddSprite("Assets/imgs/heroi.png");
Entity.AddText("Olá!", 24f, 1f, 1f, 1f);
Entity.AddAudio("Assets/sons/pulo.wav");
Entity.AddCamera();
Entity.AddLight(LightType.Directional, r, g, b, intensidade);
Entity.AddMeshRenderer("builtin:cube");
Entity.AddAnimator("Assets/Models/Fox.glb");
// Animação de sprite (sprite sheet) em runtime
Entity.AddSpriteAnimation("Assets/imgs/run.png", fps: 12f, totalFrames: 8, framesPerRow: 4);
// Partículas configuráveis em runtime
Entity.SetParticleRate(60f);
Entity.SetParticleLifetime(0.5f, 1.5f);
Entity.SetParticleVelocity(min, max);
Entity.SetParticleGravity(new Math.Vector3(0, -2, 0));
Entity.SetParticleColors(startColor, endColor);
Entity.SetParticleSize(0.2f, 0.5f);
```

## Tilemap procedural

```csharp
var mapa = Scene.CreateEntity("Mapa");
mapa.AddTilemap("Assets/imgs/atlas.png", atlasCols: 8, atlasRows: 8, mapW: 20, mapH: 12);
mapa.AddSolidTile(3);   // tile 3 do atlas = sólido (collider Box2D)
mapa.SetTile(2, 3, 3);  // põe o tile no mapa
```

## Física

```csharp
Entity.AddRigidbody2D();
Entity.AddBoxCollider2D();
Entity.AddCircleCollider2D();
Entity.AddRigidbody3D();
Entity.AddBoxCollider3D();
Entity.AddSphereCollider3D();
Entity.SetLinearVelocity(x, y);          // 2D
Entity.ApplyForce(x, y, z);              // 3D
Entity.ApplyImpulse(x, y, z);
Entity.TryGetVelocity(out vx, out vy, out vz);
Entity.SetVelocity(vx, vy, vz);
```

## Material e luz

```csharp
Entity.SetMaterial(r, g, b, metallic, roughness);
Entity.SetMaterialAlbedoMap(path);
Entity.SetMaterialNormalMap(path);
Entity.SetMaterialHeightMap(path);       // POM
Entity.SetMaterialEmissive(r, g, b, strength);
Entity.SetLightColor(r, g, b);
Entity.SetLightIntensity(x);
```

## Sistemas de engine (0.8.0)

```csharp
// Character Controller (gravidade + chão por raycast)
Entity.AddCharacterController(6f, -20f);
Entity.MoveCharacter(dirX, dirZ);

// Timeline (cutscene): keyframes de posição interpolados
Entity.AddTimeline();
Entity.AddTimelineKeyframe(0f, new Math.Vector3(0, 1, 0));
Entity.AddTimelineKeyframe(3f, new Math.Vector3(5, 1, 0));
Entity.PlayTimeline();
```

No editor: componentes **LOD** (malhas por distância), **Terreno** (heightmap
procedural) e **Timeline** — veja o Inspetor.

## Câmera de jogo (0.8.0)

```csharp
// Câmera segue a entidade com o nome dado, com suavidade e offset
Entity.AddCameraFollow("Jogador", new Math.Vector3(0f, 3f, -6f), 8f);
Entity.SetCameraFollowTarget("Jogador");   // troca de alvo em runtime
Entity.SetCameraFollowOffset(new Math.Vector3(0f, 2f, -4f));
Entity.SetCameraFollowSmoothness(5f);
```

No editor: componente **Camera Follow** (Inspetor) com offset, suavidade,
"gira com o alvo" e offset em espaço mundo.

## Tags & Layers (filtro de colisão)

```csharp
Entity.Layer = 1;                  // camada da entidade (0..15)
Entity.CollisionMask = 0xFFFFFFFF; // camadas com que colide (bitmask)
Entity.SetCollideWithLayers(1, 2); // colide só com as camadas 1 e 2
```

A física (2D e 3D) respeita a camada e a máscara: entidades de camadas
diferentes que não se colidem atravessam umas às outras.

## Duplicar e instanciar

```csharp
Entity.Duplicate();
Scene.InstantiatePrefab("Assets/Prefabs/inimigo.kzprefab", x, y, z);
```

Veja também [Scene](scene.html), [Física API](fisica-api.html) e
[UI API](ui-api.html).
