#pragma once


#include "xdr/vii-types.h"
#include <vector>

namespace viichain
{

struct SCPEnvelope;
struct SCPStatement;
struct StellarValue;

std::vector<Hash> getTxSetHashes(SCPEnvelope const& envelope);
std::vector<StellarValue> getStellarValues(SCPStatement const& envelope);
}
