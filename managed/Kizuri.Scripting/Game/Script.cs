// NativeScript — equivalente managed do NativeScript C++: a classe base que o
// jogo herda para receber callback de gameplay.
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

	// Chamado pelo host (Host.UpdateScript) depois de cada OnUpdate.
	internal void UpdateCoroutines(float deltaSeconds)
	{
		for (int i = m_Coroutines.Count - 1; i >= 0; --i)
		{
			if (m_Coroutines[i].Tick(deltaSeconds)) m_Coroutines.RemoveAt(i);
		}
	}
}
