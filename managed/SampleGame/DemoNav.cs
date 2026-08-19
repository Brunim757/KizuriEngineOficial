using Kizuri;
using Kizuri.Math;

public sealed class DemoNav : Script
{
	private Entity _agent;
	private bool _goA;

	public override void OnCreate()
	{

		var grid = Scene.CreateEntity("Grade");
		grid.AddNavGrid(-15f, -15f, 30, 30, 1f);

		var obstacle = Scene.CreateEntity("Muro");
		obstacle.AddNavObstacle(2f, 2f, 0.5f);
		obstacle.SetPosition(new Vector3(0f, 0.5f, 0f));

		_agent = Scene.CreateEntity("Agente");
		_agent.AddNavAgent(3f, 6f);
		_agent.SetPosition(new Vector3(-10f, 0.5f, 0f));
		_goA = true;
		_agent.SetNavDestination(10f, 0.5f, 0f);
	}

	public override void OnUpdate(float deltaSeconds)
	{

		if (_agent.NavAgentHasPath() == false)
		{
			_goA = !_goA;
			_agent.SetNavDestination(_goA ? 10f : -10f, 0.5f, _goA ? 0f : 0f);
			Log.Info($"DemoNav: agente chegou, indo pra {( _goA ? "+X" : "-X")} | restante: {_agent.NavAgentRemainingDistance():0.0}");
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}
