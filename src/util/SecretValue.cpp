
#include "util/SecretValue.h"

namespace viichain
{

bool
operator==(SecretValue const& x, SecretValue const& y)
{
    return x.value == y.value;
}

bool
operator!=(SecretValue const& x, SecretValue const& y)
{
    return !(x == y);
}
}
