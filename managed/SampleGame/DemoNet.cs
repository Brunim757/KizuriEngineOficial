using Kizuri;
using Kizuri.Math;
using System.Text;

public sealed class DemoNet : Script
{
	private bool _host = true;
	private float _timer = 1f;

	public override void OnCreate()
	{

		_host = Network.Host(26000);
		Log.Info($"DemoNet: {( _host ? $"HOST na porta {26000}" : "cliente (não conseguiu hostar)")}");
	}

	public override void OnUpdate(float deltaSeconds)
	{

		NetEvent ev;
		while (Network.PollEvent(out ev))
		{
			switch (ev.Type)
			{
				case NetEventType.Connect:
					Log.Info($"DemoNet: jogador {ev.Peer} conectou!");
					break;
				case NetEventType.Data:
					Log.Info($"DemoNet: mensagem do jogador {ev.Peer}: {Encoding.UTF8.GetString(ev.Data)}");
					break;
			}
		}

		_timer -= deltaSeconds;
		if (_timer <= 0f)
		{
			_timer = 1f;
			byte[] msg = Encoding.UTF8.GetBytes("ping " + Rand.Int(0, 1000));
			Network.Send(1, msg);
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}
