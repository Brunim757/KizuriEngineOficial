# Kizuri Engine v2 CSharp Scripting

## PT-BR

### Objetivo

O sistema de scripting da `Kizuri Engine v2` usa `C#` como linguagem principal de gameplay. O objetivo e dar ergonomia alta para criacao de jogos sem comprometer a performance do runtime nativo.

### Modelo

- `C++` no kernel da engine.
- `C#` para scripts de gameplay, ferramentas de alto nivel e automacao.
- `CoreCLR` hospedado pela engine.
- Assemblies do projeto compiladas separadamente do runtime nativo.

### Regras de design

- Scripts nao devem falar com memoria nativa arbitraria.
- Toda API exposta ao `C#` deve passar por contratos estaveis e documentados.
- Chamadas frequentes precisam ser enxutas e previsiveis.
- Hot reload e recarga de assemblies devem ser explicitamente controlados pelo editor.

### API inicial desejada

- `Entity`
- `Transform`
- `Camera`
- `SpriteRenderer`
- `MeshRenderer`
- `AudioSource`
- `Rigidbody2D`
- `Rigidbody3D`
- `Input`
- `Time`
- `Scene`

### Fluxo esperado

1. Usuario cria um projeto.
2. O editor gera ou atualiza a solucao de scripts.
3. O workflow compila assemblies `C#`.
4. A engine carrega os assemblies e registra tipos de script.
5. O editor permite anexar scripts a entidades.
6. Em `Play`, a instancia gerenciada interage com o runtime nativo.

### Exemplo de direcao de API

```csharp
using Kizuri;

public sealed class PlayerController : EntityScript
{
    public float MoveSpeed = 4.0f;

    public override void OnCreate()
    {
    }

    public override void OnUpdate(float deltaTime)
    {
        var move = Vector3.Zero;

        if (Input.IsKeyDown(KeyCode.W)) move.Z -= 1.0f;
        if (Input.IsKeyDown(KeyCode.S)) move.Z += 1.0f;
        if (Input.IsKeyDown(KeyCode.A)) move.X -= 1.0f;
        if (Input.IsKeyDown(KeyCode.D)) move.X += 1.0f;

        Transform.Position += move * MoveSpeed * deltaTime;
    }
}
```

### O que evitar

- Bridge baseada em ABI `C++` fragil.
- Reflection pesada por frame.
- API inchada cedo demais.
- Misturar tooling de editor e runtime sem fronteiras claras.

---

## EN

### Goal

The `Kizuri Engine v2` scripting system uses `C#` as the main gameplay language. The goal is to provide high ergonomics for game creation without compromising native runtime performance.

### Model

- `C++` in the engine kernel.
- `C#` for gameplay scripts, high-level tools, and automation.
- `CoreCLR` hosted by the engine.
- Project assemblies compiled separately from the native runtime.

### Design rules

- Scripts must not access arbitrary native memory.
- Every API exposed to `C#` must go through stable, documented contracts.
- Frequent calls need to stay lean and predictable.
- Hot reload and assembly reload must be explicitly controlled by the editor.

### Desired initial API

- `Entity`
- `Transform`
- `Camera`
- `SpriteRenderer`
- `MeshRenderer`
- `AudioSource`
- `Rigidbody2D`
- `Rigidbody3D`
- `Input`
- `Time`
- `Scene`

### Expected flow

1. The user creates a project.
2. The editor generates or updates the script solution.
3. The workflow compiles `C#` assemblies.
4. The engine loads assemblies and registers script types.
5. The editor lets users attach scripts to entities.
6. During `Play`, managed instances interact with the native runtime.

### Directional API example

```csharp
using Kizuri;

public sealed class PlayerController : EntityScript
{
    public float MoveSpeed = 4.0f;

    public override void OnCreate()
    {
    }

    public override void OnUpdate(float deltaTime)
    {
        var move = Vector3.Zero;

        if (Input.IsKeyDown(KeyCode.W)) move.Z -= 1.0f;
        if (Input.IsKeyDown(KeyCode.S)) move.Z += 1.0f;
        if (Input.IsKeyDown(KeyCode.A)) move.X -= 1.0f;
        if (Input.IsKeyDown(KeyCode.D)) move.X += 1.0f;

        Transform.Position += move * MoveSpeed * deltaTime;
    }
}
```

### What to avoid

- Bridge based on fragile `C++` ABI coupling.
- Heavy per-frame reflection.
- Overgrown API too early.
- Mixing editor tooling and runtime concerns without clear boundaries.
