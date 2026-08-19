#pragma once

#include "kizuri/ecs/Entity.hpp"
#include <cstdint>

namespace kizuri {
namespace scripting {

uint32_t RegisterEntityHandle(Entity entity);

float GetTimeScale();
void SetTimeScale(float scale);

}
}
