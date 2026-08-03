// Host.cs — ponte host <-> managed. Estas funções são chamadas DIRETAMENTE
// pela engine via load_assembly_and_get_function_pointer (CoreCLRHost.cpp);
// as assinaturas precisam casar 1:1 com os typedefs de lá. Os delegates
// públicos são usados como "delegate_type_name" para o marshalling.
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Kizuri.Hosting;

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void InitializeGameModuleFn([MarshalAs(UnmanagedType.LPUTF8Str)] string gameAssemblyPath);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate int GetScriptCountFn();

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate int GetScriptNameFn(int index, IntPtr buffer, int bufferSize);

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
	// Invocado pelo host assim que o runtime sobe: carrega o assembly do
	// jogo (mesmo contexto do host — já foi resolvido) e chama os métodos
	// marcados com [GameEntryPoint] para registrar os scripts.
	public static void InitializeGameModule([MarshalAs(UnmanagedType.LPUTF8Str)] string gameAssemblyPath)
	{
		try
		{
			var asm = Assembly.LoadFrom(gameAssemblyPath);
			foreach (var type in asm.GetTypes())
			{
				foreach (var method in type.GetMethods(BindingFlags.Public | BindingFlags.Static))
				{
					if (method.GetCustomAttribute<GameEntryPointAttribute>() != null)
						method.Invoke(null, null);
				}
			}
		}
		catch (Exception ex)
		{
			Log.Error("Falha ao inicializar o módulo do jogo: " + ex);
		}
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
				s.OnUpdate(deltaSeconds);
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
