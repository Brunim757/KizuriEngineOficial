
using System.Text.Json;

namespace Kizuri;

public static class SaveSystem
{
	private static readonly Dictionary<string, object> s_Data = new();
	private static string s_Path = "save.json";
	private static bool s_Loaded;

	public static string FilePath => s_Path;

	public static void SetPath(string path)
	{
		s_Path = string.IsNullOrWhiteSpace(path) ? "save.json" : path;
		s_Loaded = false;
		s_Data.Clear();
	}

	public static void Set(string key, string value) => s_Data[key] = value;
	public static void Set(string key, int value) => s_Data[key] = value;
	public static void Set(string key, float value) => s_Data[key] = value;
	public static void Set(string key, bool value) => s_Data[key] = value;
	public static void Set(string key, Math.Vector2 value) => s_Data[key] = $"{value.X};{value.Y}";
	public static void Set(string key, Math.Vector3 value) => s_Data[key] = $"{value.X};{value.Y};{value.Z}";

	public static bool Has(string key) { EnsureLoaded(); return s_Data.ContainsKey(key); }

	public static string GetString(string key, string fallback = "")
	{
		EnsureLoaded();
		return s_Data.TryGetValue(key, out var v) && v is string s ? s : fallback;
	}

	public static int GetInt(string key, int fallback = 0)
	{
		EnsureLoaded();
		if (s_Data.TryGetValue(key, out var v))
		{
			if (v is int i) return i;
			if (v is float f) return (int)f;
		}
		return fallback;
	}

	public static float GetFloat(string key, float fallback = 0f)
	{
		EnsureLoaded();
		if (s_Data.TryGetValue(key, out var v))
		{
			if (v is float f) return f;
			if (v is int i) return i;
		}
		return fallback;
	}

	public static bool GetBool(string key, bool fallback = false)
	{
		EnsureLoaded();
		return s_Data.TryGetValue(key, out var v) && v is bool b ? b : fallback;
	}

	public static Math.Vector2 GetVector2(string key, Math.Vector2 fallback = default)
	{
		var parts = GetString(key).Split(';');
		return parts.Length == 2 && float.TryParse(parts[0], out var x) && float.TryParse(parts[1], out var y)
			? new Math.Vector2(x, y) : fallback;
	}

	public static Math.Vector3 GetVector3(string key, Math.Vector3 fallback = default)
	{
		var parts = GetString(key).Split(';');
		return parts.Length == 3
			&& float.TryParse(parts[0], out var x) && float.TryParse(parts[1], out var y) && float.TryParse(parts[2], out var z)
			? new Math.Vector3(x, y, z) : fallback;
	}

	public static void Save()
	{
		try { File.WriteAllText(s_Path, JsonSerializer.Serialize(s_Data)); }
		catch (Exception ex) { Log.Error("Falha ao salvar: " + ex); }
	}

	public static void Load()
	{
		s_Data.Clear();
		try
		{
			if (!File.Exists(s_Path)) { s_Loaded = true; return; }
			using var doc = JsonDocument.Parse(File.ReadAllText(s_Path));
			foreach (var prop in doc.RootElement.EnumerateObject())
			{
				var value = ConvertValue(prop.Value);
				if (value != null) s_Data[prop.Name] = value;
			}
		}
		catch (Exception ex) { Log.Error("Falha ao carregar save: " + ex); }
		s_Loaded = true;
	}

	private static object? ConvertValue(JsonElement e)
	{
		switch (e.ValueKind)
		{
			case JsonValueKind.String: return e.GetString();
			case JsonValueKind.True: return true;
			case JsonValueKind.False: return false;
			case JsonValueKind.Number:
				return e.TryGetInt32(out var i) ? i : (object)e.GetSingle();
			default: return null;
		}
	}

	private static void EnsureLoaded()
	{
		if (!s_Loaded) Load();
	}
}
