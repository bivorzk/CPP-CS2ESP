#pragma once

#include <cstdint>
#include "mem.hpp"
#include "offsets.hpp"

namespace Bomb {

struct Info {
    bool      valid = false;
    uintptr_t plantedC4ClassPointer = 0;
    float     blowTime = 0.0f;
    bool      active = false;      // planted/c4 ticking state

    struct Vec3 { float x, y, z; } origin{};
};

class Finder {
public:
    static constexpr uintptr_t PlantedC4Offset = Offsets::dwPlantedC4::STATIC_PTR;

    static Info read(mem::ProcessMemory* proc);
};

} // namespace Bomb
