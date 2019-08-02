
#pragma once

namespace viichain
{
void assertThreadIsMain();

void dbgAbort();

[[noreturn]] void printErrorAndAbort(const char* s1);
[[noreturn]] void printErrorAndAbort(const char* s1, const char* s2);

#ifdef NDEBUG

#define dbgAssert(expression) ((void)0)

#else

#define dbgAssert(expression) (void)((!!(expression)) || (dbgAbort(), 0))

#endif
}
