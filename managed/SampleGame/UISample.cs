using Kizuri;
using Kizuri.Math;
using System.Collections;

// Demonstra o sistema de UI + utilidades de gameplay: um Canvas com botão
// clicável, HUD de texto, corrotinas e TimeScale.
public sealed class UISample : Script
{
	private Entity _canvas;
	private Entity _button;
	private Entity _counterText;
	private int _clicks;
	private float _announceTimer;

	public override void OnCreate()
	{
		// Canvas raiz: filhos com UIRect são desenhados em espaço de tela.
		_canvas = Scene.CreateEntity("UI Canvas");
		_canvas.AddUICanvas(10f);

		var title = Scene.CreateEntity("Titulo");
		title.SetParent(_canvas);
		title.AddUIText("KIZURI ENGINE", 0.9f, 1f, 1f, 1f);
		title.SetUIRect(0f, 7.5f, 0f, 0f);

		// Botão = rect com fundo + texto no centro (mesma entidade).
		_button = Scene.CreateEntity("Botao");
		_button.SetParent(_canvas);
		_button.AddUIButton(0f, 1f, 6f, 2f, 0.22f, 0.42f, 0.9f);
		_button.AddUIText("Clique em mim", 0.6f, 1f, 1f, 1f);

		// HUD de pontuação (texto sem fundo).
		_counterText = Scene.CreateEntity("Contador");
		_counterText.SetParent(_canvas);
		_counterText.AddUIText("Cliques: 0", 0.55f, 1f, 0.95f, 0.5f);
		_counterText.SetUIRect(0f, -2.5f, 0f, 0f);
	}

	public override void OnUpdate(float deltaSeconds)
	{
		// Clique no botão: incrementa, pisca a cor via corrotina e toca som.
		if (_button.UIButtonWasClicked())
		{
			_clicks++;
			Log.Info($"Botão clicado! Total: {_clicks}");
			StartCoroutine(Pisca());
		}

		_counterText.SetText($"Cliques: {_clicks}");

		// TimeScale (câmera lenta) com F; volta ao normal com G.
		if (Input.IsKeyPressed(Key.F)) Time.TimeScale = 0.3f;
		else if (Input.IsKeyPressed(Key.G)) Time.TimeScale = 1f;

		// Exemplo de corrotina periódica + Rand.
		_announceTimer -= deltaSeconds;
		if (_announceTimer <= 0f)
		{
			_announceTimer = 3f;
			StartCoroutine(Anuncia());
		}
	}

	private IEnumerator Pisca()
	{
		_button.SetUIColor(1f, 0.6f, 0.2f);
		yield return new WaitForSeconds(0.15f);
		_button.SetUIColor(0.22f, 0.42f, 0.9f);
	}

	private IEnumerator Anuncia()
	{
		yield return new WaitForSeconds(1f);
		Log.Info($"Rand.Float(0,100) = {Rand.Float(0f, 100f):0.00} | Chance(0.5) = {Rand.Chance(0.5f)} | Mathf.Clamp(7,0,5) = {Mathf.Clamp(7, 0, 5)}");
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}
