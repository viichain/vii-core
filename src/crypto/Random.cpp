
#include "crypto/Random.h"
#include <sodium.h>

#ifdef MSAN_ENABLED
#include <sanitizer/msan_interface.h>
#endif

std::vector<uint8_t>
randomBytes(size_t length)
{
    std::vector<uint8_t> vec(length);
    randombytes_buf(vec.data(), length);
#ifdef MSAN_ENABLED
    __msan_unpoison(out.key.data(), out.key.size());
#endif
    return vec;
}
