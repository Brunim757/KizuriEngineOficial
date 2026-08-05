---
title: Exemplos completos
group: Scripting C#
order: 13
---

# Exemplos completos

Exemplos reais que vêm no repositório em `managed/SampleGame/`.

## PlayerController — movimento, projéteis, HUD, save e raycast

```csharp
using Kizuri;
using Kizuri.Math;

public sealed class PlayerController : Script
{
    private const float Speed = 5.0f;

    private Entity _hud;
    private readonly List<Bullet> _bullets = new();
    private float _score;
    private bool _mouseDown;
    private float _raycastTimer;

    private struct Bullet
    {
        public Entity Entity;
        public Vector3 Velocity;
        public float Life;
    }

    public override void OnCreate()
    {
        _hud = Scene.CreateEntity("HUD");
        _hud.AddText("Pontos: 0", 32f);
        _hud.SetPosition(new Vector3(-8f, 5f, 0f));
        _score = SaveSystem.GetFloat("score", 0f);   // auto-carrega
    }

    public override void OnUpdate(float deltaSeconds)
    {
        var wish = Vector2.Zero;
        if (Input.IsKeyPressed(Key.A) || Input.IsKeyPressed(Key.Left))  wish.X -= 1f;
        if (Input.IsKeyPressed(Key.D) || Input.IsKeyPressed(Key.Right)) wish.X += 1f;
        if (Input.IsKeyPressed(Key.W) || Input.IsKeyPressed(Key.Up))    wish.Y += 1f;
        if (Input.IsKeyPressed(Key.S) || Input.IsKeyPressed(Key.Down))  wish.Y -= 1f;

        if (wish.Length > 0.001f) wish = wish / wish.Length * Speed;

        if (Entity.TryGetRigidbody2D(out var rb))
        {
            var v = rb.GetLinearVelocity();
            rb.SetLinearVelocity(new Vector2(wish.X, v.Y));
        }
        else if (Entity.TryGetTransform(out var t))
        {
            t.Translation.X += wish.X * deltaSeconds;
            t.Translation.Y += wish.Y * deltaSeconds;
            Entity.SetPosition(t.Translation);
        }

        // Atirar no clique (uma vez por clique)
        if (Input.IsMouseButtonPressed(MouseButton.Left) && !_mouseDown)
        {
            _mouseDown = true;
            if (Entity.TryGetTransform(out var tp))
            {
                var bullet = Scene.CreateEntity("Bullet");
                bullet.AddSprite();
                bullet.SetSpriteColor(1f, 0.85f, 0.2f);
                bullet.SetPosition(tp.Translation);
                _bullets.Add(new Bullet { Entity = bullet,
                                          Velocity = new Vector3(8f, 0f, 0f),
                                          Life = 1.5f });
            }
        }
        if (!Input.IsMouseButtonPressed(MouseButton.Left)) _mouseDown = false;

        // Atualizar projéteis
        for (int i = _bullets.Count - 1; i >= 0; --i)
        {
            var b = _bullets[i];
            b.Life -= deltaSeconds;
            if (b.Life <= 0f)
            {
                b.Entity.Destroy();
                _bullets.RemoveAt(i);
                continue;
            }
            if (b.Entity.TryGetTransform(out var bt))
            {
                bt.Translation += b.Velocity * deltaSeconds;
                b.Entity.SetPosition(bt.Translation);
            }
            _bullets[i] = b;
        }

        // HUD + save (F5)
        _score += deltaSeconds * 10f;
        _hud.SetText($"Pontos: {(int)_score} | Projéteis: {_bullets.Count}");
        if (Input.IsKeyPressed(Key.F5))
        {
            SaveSystem.Set("score", _score);
            SaveSystem.Save();
            Log.Info("Jogo salvo.");
        }

        // Raycast 2D de exemplo (a cada 0.5s)
        _raycastTimer -= deltaSeconds;
        if (_raycastTimer <= 0f)
        {
            _raycastTimer = 0.5f;
            if (Entity.TryGetTransform(out var rt))
            {
                var from = new Vector2(rt.Translation.X, rt.Translation.Y);
                var to = new Vector2(rt.Translation.X, rt.Translation.Y - 20f);
                if (Scene.Raycast2D(from, to, out var hit, out var point))
                    Log.Info($"Raycast acertou {hit.Id} em ({point.X:0.00}, {point.Y:0.00}).");
            }
        }
    }

    public override void OnCollisionBegin(Entity other) { }
    public override void OnCollisionEnd(Entity other) { }
    public override void OnDestroy() { }
}
```

## Registro dos scripts

```csharp
public static class SampleGameModule
{
    [Kizuri.GameEntryPoint]
    public static void RegisterAll()
    {
        Kizuri.GameModule.Register<PlayerController>("PlayerController");
        Kizuri.GameModule.Register<UISample>("UISample");
        Kizuri.GameModule.Register<Demo3D>("Demo3D");
    }
}
```

## UISample — botão interativo

```csharp
public sealed class UISample : Script
{
    private Entity _botao;
    private Entity _rotulo;
    private int _cliques;

    public override void OnCreate()
    {
        var canvas = Scene.CreateEntity("Canvas");
        canvas.AddUICanvas(10f);

        _botao = Scene.CreateEntity("Botão");
        _botao.SetParent(canvas);
        _botao.AddUIButton(0f, -3f, 5f, 1.2f, 0.82f, 0.24f, 0.27f);

        _rotulo = Scene.CreateEntity("Rótulo");
        _rotulo.SetParent(canvas);
        _rotulo.AddUIText("Cliques: 0", 0.6f, 1f, 1f, 1f);
        _rotulo.SetUIRect(0f, -1.6f, 6f, 1f);
    }

    public override void OnUpdate(float deltaSeconds)
    {
        if (_botao.UIButtonWasClicked())
        {
            _cliques++;
            _rotulo.SetText($"Cliques: {_cliques}");
        }
        _botao.SetUIColor(_botao.UIButtonIsHovered() ? 1f : 0.82f,
                          1f : 0.24f, 1f : 0.27f, 1f);
    }
}
```

::: info
Versões destes exemplos (com física, corrotinas e look-at 3D) estão em
`managed/SampleGame/` no repositório.
:::
