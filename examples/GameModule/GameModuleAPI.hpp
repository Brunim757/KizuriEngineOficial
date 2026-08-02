#pragma once

// __declspec(dllexport) no Windows: sem isso o símbolo RegisterScripts fica
// interno à DLL e o ScriptEngine (LoadLibrary/GetProcAddress) não consegue
// achar ele pelo nome. No Linux/macOS não precisa de nada especial — os
// símbolos de uma .so já são visíveis por padrão.
#if defined(_WIN32)
    #define KZ_GAME_MODULE_API __declspec(dllexport)
#else
    #define KZ_GAME_MODULE_API
#endif
