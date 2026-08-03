// Coroutine — execução diferida no estilo Unity: StartCoroutine com
// `yield return new WaitForSeconds(...)` ou `WaitForFrames(...)`. O host
// (Kizuri.Hosting.Host.UpdateScript) chama UpdateCoroutines depois de cada
// OnUpdate, então o jogo não precisa gerenciar nada.
using System.Collections;

namespace Kizuri;

public abstract class Script
{
	public Entity Entity { get; internal set; }
	public abstract void OnCreate();
	public abstract void OnUpdate(float deltaSeconds);
	public abstract void OnCollisionBegin(Entity other);
	public abstract void OnCollisionEnd(Entity other);
	public abstract void OnDestroy();

	private readonly List<Coroutine> m_Coroutines = new();

	// Inicia uma corrotina nesta instância do script. Ex:
	//   StartCoroutine(MinhaRotina());
	//   IEnumerator MinhaRotina() {
	//       yield return new WaitForSeconds(1f);
	//       Log.Info("um segundo depois...");
	//   }
	protected void StartCoroutine(IEnumerator routine)
	{
		if (routine != null) m_Coroutines.Add(new Coroutine(routine));
	}

	internal void UpdateCoroutines(float deltaSeconds)
	{
		for (int i = m_Coroutines.Count - 1; i >= 0; --i)
		{
			if (m_Coroutines[i].Tick(deltaSeconds)) m_Coroutines.RemoveAt(i);
		}
	}
}

public sealed class Coroutine
{
	private readonly IEnumerator m_Enumerator;
	private float m_WaitSeconds;
	private int m_WaitFrames;

	public Coroutine(IEnumerator enumerator) => m_Enumerator = enumerator;

	// true = corrotina terminou.
	public bool Tick(float deltaSeconds)
	{
		if (m_WaitSeconds > 0f) { m_WaitSeconds -= deltaSeconds; return false; }
		if (m_WaitFrames > 0) { --m_WaitFrames; return false; }
		if (!m_Enumerator.MoveNext()) return true;
		switch (m_Enumerator.Current)
		{
			case WaitForSeconds w: m_WaitSeconds = w.Seconds; break;
			case WaitForFrames f: m_WaitFrames = f.Frames; break;
		}
		return false;
	}
}

public sealed class WaitForSeconds
{
	public float Seconds;
	public WaitForSeconds(float seconds) => Seconds = seconds;
}

public sealed class WaitForFrames
{
	public int Frames;
	public WaitForFrames(int frames) => Frames = frames;
}
