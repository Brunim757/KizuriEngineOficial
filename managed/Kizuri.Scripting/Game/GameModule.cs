
namespace Kizuri;

public delegate Script ScriptFactory();

public static class GameModule
{

	private static readonly Dictionary<string, ScriptFactory> s_Factories = new();

	public static void Register<T>(string name) where T : Script, new()
		=> Register(name, () => new T());

	public static void Register(string name, ScriptFactory factory)
		=> s_Factories[name] = factory;

	public static bool Exists(string name) => s_Factories.ContainsKey(name);
	public static Script? Create(string name) =>
		s_Factories.TryGetValue(name, out var f) ? f() : null;
	public static string[] ScriptNames => s_Factories.Keys.ToArray();
}