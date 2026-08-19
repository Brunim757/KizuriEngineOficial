
namespace Kizuri;

public static class Log
{
	internal const int LevelTrace = 0, LevelDebug = 1, LevelInfo = 2, LevelWarn = 3, LevelError = 4, LevelCritical = 5;

	internal static void Core(int level, string msg) => Interop.KizuriNative.kz_log(0, level, msg);

	public static void Info(string msg)  => Core(LevelInfo, msg);
	public static void Warn(string msg)  => Core(LevelWarn, msg);
	public static void Error(string msg) => Core(LevelError, msg);
}