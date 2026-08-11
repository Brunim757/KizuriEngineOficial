using System;

namespace Kizuri
{
	// Rede multiplayer (pilar AAA v0.34): uma sessão por processo, atualizada
	// automaticamente pela engine. Host numa instância do jogo, clientes nas
	// outras (porta padrão 26000). Eventos consultados por PollEvent.
	public enum NetEventType : int { Connect = 0, Disconnect = 1, Data = 2, Error = 3 }

	public struct NetEvent
	{
		public NetEventType Type;
		public uint Peer;
		public byte[] Data;
	}

	public static class Network
	{
		public static bool Host(ushort port = 26000)
			=> Interop.KizuriNative.kz_net_host(port) != 0;
		public static bool Connect(string address, ushort port = 26000)
			=> Interop.KizuriNative.kz_net_connect(address, port) != 0;
		public static void Shutdown()
			=> Interop.KizuriNative.kz_net_shutdown();
		public static bool IsHost
			=> Interop.KizuriNative.kz_net_is_host() != 0;

		public static bool Send(uint peer, byte[] data)
			=> Interop.KizuriNative.kz_net_send(peer, data, (uint)data.Length) != 0;

		// Próximo evento pendente (false se a fila está vazia).
		public static bool PollEvent(out NetEvent ev)
		{
			ev = new NetEvent();
			int type = 0;
			uint peer = 0;
			uint size = 0;
			byte[] buf = new byte[1024];
			if (Interop.KizuriNative.kz_net_poll_event(ref type, ref peer, buf, (uint)buf.Length, ref size) == 0)
				return false;
			ev.Type = (NetEventType)type;
			ev.Peer = peer;
			ev.Data = new byte[size];
			Array.Copy(buf, ev.Data, (int)size);
			return true;
		}
	}
}
