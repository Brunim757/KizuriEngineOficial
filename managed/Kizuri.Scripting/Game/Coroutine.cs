




namespace Kizuri;

public sealed class Coroutine
{
	private readonly System.Collections.IEnumerator m_Enumerator;
	private float m_WaitSeconds;
	private int m_WaitFrames;

	public Coroutine(System.Collections.IEnumerator enumerator) => m_Enumerator = enumerator;

	
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
