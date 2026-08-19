
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;

namespace Kizuri.Hosting;

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void InitializeGameModuleFn([MarshalAs(UnmanagedType.LPUTF8Str)] string gameAssemblyPath);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate int GetScriptCountFn();

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate int GetScriptNameFn(int index, IntPtr buffer, int bufferSize);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate int GetLastInitErrorFn(IntPtr buffer, int bufferSize);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate IntPtr CreateScriptFn([MarshalAs(UnmanagedType.LPUTF8Str)] string className, uint entityHandle);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void DestroyScriptFn(IntPtr handle);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void UpdateScriptFn(IntPtr handle, float deltaSeconds);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void CollisionScriptFn(IntPtr handle, uint otherHandle, int begin);

public static class Host
{

	internal static string s_LastInitError = string.Empty;

	public static void InitializeGameModule([MarshalAs(UnmanagedType.LPUTF8Str)] string gameAssemblyPath)
	{
		s_LastInitError = string.Empty;
		try
		{

			var alc = AssemblyLoadContext.GetLoadContext(typeof(Host).Assembly);
			var asm = alc.LoadFromAssemblyPath(gameAssemblyPath);
			foreach (var type in asm.GetTypes())
			{
				foreach (var method in type.GetMethods(BindingFlags.Public | BindingFlags.Static))
				{
					if (method.GetCustomAttribute<GameEntryPointAttribute>() != null)
						method.Invoke(null, null);
				}
			}

			foreach (var type in asm.GetTypes())
			{
				if (!type.IsClass || type.IsAbstract) continue;
				if (!typeof(Script).IsAssignableFrom(type)) continue;
				if (type.Namespace?.StartsWith("Kizuri") == true) continue;
				if (GameModule.Exists(type.Name)) continue;
				var t = type;
				GameModule.Register(type.Name, () => (Script)Activator.CreateInstance(t)!);
			}
		}
		catch (Exception ex)
		{

			s_LastInitError = ex is ReflectionTypeLoadException rtl
				? rtl.ToString() + "\n  -> " + string.Join("\n  -> ", rtl.LoaderExceptions.Select(e => e?.ToString()))
				: ex.ToString();
			try { Log.Error("Falha ao inicializar o módulo do jogo: " + s_LastInitError); }
			catch { }
		}
	}

	public static int GetLastInitError(IntPtr buffer, int bufferSize)
	{
		var text = s_LastInitError ?? string.Empty;
		var bytes = Encoding.UTF8.GetBytes(text);
		int copy = System.Math.Min(bytes.Length, System.Math.Max(0, bufferSize - 1));
		if (buffer != IntPtr.Zero && copy > 0)
		{
			Marshal.Copy(bytes, 0, buffer, copy);
			Marshal.WriteByte(buffer, copy, 0);
		}
		return bytes.Length;
	}

	public static int GetScriptCount()
	{
		try { return GameModule.ScriptNames.Length; }
		catch (Exception ex) { Log.Error("GetScriptCount falhou: " + ex); return 0; }
	}

	public static int GetScriptName(int index, IntPtr buffer, int bufferSize)
	{
		try
		{
			var names = GameModule.ScriptNames;
			if ((uint)index >= (uint)names.Length) return 0;
			var bytes = Encoding.UTF8.GetBytes(names[index]);
			int copy = System.Math.Min(bytes.Length, System.Math.Max(0, bufferSize - 1));
			if (buffer != IntPtr.Zero && copy > 0)
			{
				Marshal.Copy(bytes, 0, buffer, copy);
				Marshal.WriteByte(buffer, copy, 0);
			}
			return bytes.Length;
		}
		catch (Exception ex) { Log.Error("GetScriptName falhou: " + ex); return 0; }
	}

	public static IntPtr CreateScript([MarshalAs(UnmanagedType.LPUTF8Str)] string className, uint entityHandle)
	{
		try
		{
			var script = GameModule.Create(className);
			if (script is null) return IntPtr.Zero;
			script.Entity = new Entity(entityHandle);
			script.OnCreate();
			return GCHandle.ToIntPtr(GCHandle.Alloc(script));
		}
		catch (Exception ex)
		{
			Log.Error($"Falha ao criar o script '{className}': {ex}");
			return IntPtr.Zero;
		}
	}

	public static void DestroyScript(IntPtr handle)
	{
		if (handle == IntPtr.Zero) return;
		try
		{
			var gc = GCHandle.FromIntPtr(handle);
			if (gc.Target is Script s) s.OnDestroy();
			gc.Free();
		}
		catch (Exception ex) { Log.Error("Falha ao destruir script: " + ex); }
	}

	public static void UpdateScript(IntPtr handle, float deltaSeconds)
	{
		if (handle == IntPtr.Zero) return;
		try
		{
			if (GCHandle.FromIntPtr(handle).Target is Script s)
			{
				s.OnUpdate(deltaSeconds);
				s.UpdateCoroutines(deltaSeconds);
			}
		}
		catch (Exception ex) { Log.Error("Falha no OnUpdate do script: " + ex); }
	}

	public static void CollisionScript(IntPtr handle, uint otherHandle, int begin)
	{
		if (handle == IntPtr.Zero) return;
		try
		{
			if (GCHandle.FromIntPtr(handle).Target is not Script s) return;
			var other = new Entity(otherHandle);
			if (begin != 0) s.OnCollisionBegin(other);
			else s.OnCollisionEnd(other);
		}
		catch (Exception ex) { Log.Error("Falha no callback de colisão do script: " + ex); }
	}
}
