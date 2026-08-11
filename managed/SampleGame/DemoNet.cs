using Kizuri;
using Kizuri.Math;
using System.Text;

// Demonstra a rede multiplayer (pilar AAA v0.34): roda o jogo como HOST
// (porta 26000) ou conecta num host por argumento. A cada segundo troca
// uma mensagem de ping entre as instâncias.
public sealed class DemoNet : Script
{
	private bool _host = true;
	private float _timer = 1f;

	public override void OnCreate()
	{
		// --host ou --connect <ip> pelo CommandLineArgs (não exposto ao C#
		// ainda — por ora, quem abre primeiro é host e quem conectar é cliente).
		_host = Network.Host(26000);
		Log.Info($"DemoNet: {( _host ? $"HOST na porta {26000}" : "cliente (não conseguiu hostar)")}");
	}

	public override void OnUpdate(float deltaSeconds)
	{
		// Processa eventos da rede.
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

		// Ping periódico (se houver alguém conectado).
		_timer -= deltaSeconds;
		if (_timer <= 0f)
		{
			_timer = 1f;
			byte[] msg = Encoding.UTF8.GetBytes("ping " + Rand.Int(0, 1000));
			Network.Send(1, msg); // peer 1 = o host (cliente) ou o 1º jogador
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}
