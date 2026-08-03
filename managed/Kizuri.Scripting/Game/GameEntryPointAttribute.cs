// GameEntryPointAttribute — marca o método estático público que registra os
// scripts do jogo (chamadas GameModule.Register). O host C++ (CoreCLRHost)
// o invoca ao carregar o assembly, exatamente como o antigo
// RegisterScripts(ScriptRegistry&) do módulo C++.
namespace Kizuri;

[AttributeUsage(AttributeTargets.Method)]
public sealed class GameEntryPointAttribute : Attribute { }
