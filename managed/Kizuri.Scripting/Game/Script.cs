// NativeScript — equivalente managed do NativeScript C++: a classe base que o
// jogo herda para receber callback de gameplay.
namespace Kizuri;

public abstract class Script
{
	public Entity Entity { get; internal set; }
	public abstract void OnCreate();
	public abstract void OnUpdate(float deltaSeconds);
	public abstract void OnCollisionBegin(Entity other);
	public abstract void OnCollisionEnd(Entity other);
	public abstract void OnDestroy();
}