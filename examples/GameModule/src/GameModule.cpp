#include <Kizuri.hpp>

#include "GameModuleAPI.hpp"

#include "RotatorScript.hpp"
#include "PlayerController.hpp"
#include "BouncerScript.hpp"

// extern "C" evita o name mangling do C++ — sem isso o nome exportado viraria
// algo como "?RegisterScripts@@YAXAEAVScriptRegistry@@@Z" em vez de
// "RegisterScripts", e o dlsym/GetProcAddress do ScriptEngine não acharia.
//
// KZ_GAME_MODULE_API garante a visibilidade do símbolo de dentro da DLL no
// Windows (ver GameModuleAPI.hpp).
extern "C" KZ_GAME_MODULE_API void RegisterScripts(kizuri::ScriptRegistry& registry) {
    registry.Register<RotatorScript>("RotatorScript");
    registry.Register<PlayerController>("PlayerController");
    registry.Register<BouncerScript>("BouncerScript");

    KZ_INFO("GameModule de exemplo carregado: 3 scripts registrados.");
}
