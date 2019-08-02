#pragma once


#include "ByteSlice.h"

namespace viichain
{

namespace shortHash
{
void initialize();
uint64_t computeHash(viichain::ByteSlice const& b);
}
}
