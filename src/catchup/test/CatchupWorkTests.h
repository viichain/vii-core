#pragma once


#include <stdint.h>
#include <utility>
#include <vector>

namespace viichain
{

class CatchupConfiguration;

extern std::vector<std::pair<uint32_t, CatchupConfiguration>>
    gCatchupRangeCases;
}
