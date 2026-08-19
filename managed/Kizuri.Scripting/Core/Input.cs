

namespace Kizuri;

public enum Key
{
	Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
	D0 = 48, D1 = 49, D2 = 50, D3 = 51, D4 = 52, D5 = 53, D6 = 54, D7 = 55, D8 = 56, D9 = 57,
	Semicolon = 59, Equal = 61,
	A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74,
	K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84,
	U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,
	LeftBracket = 91, Backslash = 92, RightBracket = 93, GraveAccent = 96,
	Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
	Right = 262, Left = 263, Down = 264, Up = 265,
	F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
	LeftShift = 340, LeftControl, LeftAlt, LeftSuper,
	RightShift = 344, RightControl, RightAlt, RightSuper,
}

public enum MouseButton
{
	Left = 0, Right = 1, Middle = 2,
}

public static class Input
{
	public static bool IsKeyPressed(Key key) => Interop.KizuriNative.kz_input_is_key_pressed((int)key) != 0;

	
	public static bool IsKeyDown(Key key) => Interop.KizuriNative.kz_input_is_key_down((int)key) != 0;

	public static bool IsMouseButtonPressed(MouseButton button)
		=> Interop.KizuriNative.kz_input_is_mouse_button_pressed((int)button) != 0;

	
	public static bool IsMouseButtonDown(MouseButton button)
		=> Interop.KizuriNative.kz_input_is_mouse_button_down((int)button) != 0;

	public static Math.Vector2 GetMousePosition()
	{
		Interop.KizuriNative.kz_input_get_mouse_position(out var x, out var y);
		return new Math.Vector2(x, y);
	}

	
	
	
	public static bool IsActionPressed(string action)
		=> Interop.KizuriNative.kz_input_is_action_pressed(action) != 0;

	public static void SetActionKey(string action, Key key)
		=> Interop.KizuriNative.kz_input_set_action_key(action, (int)key);

	public static Key? GetActionKey(string action)
	{
		int k = Interop.KizuriNative.kz_input_get_action_key(action);
		return k < 0 ? null : (Key)k;
	}
}