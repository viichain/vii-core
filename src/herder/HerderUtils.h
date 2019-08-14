#pragma once


#include "xdr/vii-types.h"
#include <vector>

namespace viichain
{

struct SCPEnvelope;
struct SCPStatement;
struct VIIValue;

std::vector<Hash> getTxSetHashes(SCPEnvelope const& envelope);
std::vector<VIIValue> getVIIValues(SCPStatement const& envelope);
}
