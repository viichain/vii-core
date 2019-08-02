#pragma once


#include "lib/util/uint128_t.h"
#include <cstdint>

namespace viichain
{
enum Rounding
{
    ROUND_DOWN,
    ROUND_UP
};

int64_t bigDivide(int64_t A, int64_t B, int64_t C, Rounding rounding);
bool bigDivide(int64_t& result, int64_t A, int64_t B, int64_t C,
               Rounding rounding);

bool bigDivide(uint64_t& result, uint64_t A, uint64_t B, uint64_t C,
               Rounding rounding);

bool bigDivide(int64_t& result, uint128_t a, int64_t B, Rounding rounding);
bool bigDivide(uint64_t& result, uint128_t a, uint64_t B, Rounding rounding);
int64_t bigDivide(uint128_t a, int64_t B, Rounding rounding);

uint128_t bigMultiply(uint64_t a, uint64_t b);
uint128_t bigMultiply(int64_t a, int64_t b);
}
