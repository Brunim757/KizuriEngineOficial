#include <Kizuri.hpp>

#include "GameModuleAPI.hpp"

#include "RotatorScript.hpp"
#include "PlayerController.hpp"
#include "BouncerScript.hpp"
#include "CollectibleScript.hpp"

extern "C" KZ_GAME_MODULE_API void RegisterScripts(kizuri::ScriptRegistry& registry) {
    registry.Register<RotatorScript>("RotatorScript");
    registry.Register<PlayerController>("PlayerController");
    registry.Register<BouncerScript>("BouncerScript");
    registry.Register<CollectibleScript>("CollectibleScript");

    KZ_INFO("GameModule de exemplo carregado: 4 scripts registrados.");
}
