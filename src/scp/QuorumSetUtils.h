
#pragma once

#include "xdr/vii-SCP.h"

namespace viichain
{
class Config;

bool isQuorumSetSane(SCPQuorumSet const& qSet, bool extraChecks);

void normalizeQSet(SCPQuorumSet& qSet, NodeID const* idToRemove = nullptr);
}
