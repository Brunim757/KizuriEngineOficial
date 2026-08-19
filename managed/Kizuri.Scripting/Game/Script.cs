
using System.Collections;

namespace Kizuri;

public abstract class Script
{
	public Entity Entity { get; internal set; }

	public virtual void OnCreate() { }
	public virtual void OnUpdate(float deltaSeconds) { }
	public virtual void OnCollisionBegin(Entity other) { }
	public virtual void OnCollisionEnd(Entity other) { }
	public virtual void OnDestroy() { }

	private readonly List<Coroutine> m_Coroutines = new();

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
