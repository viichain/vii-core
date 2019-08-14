#pragma once
#include "xdr/vii-ledger-entries.h"
#include "xdr/vii-ledger.h"
#include "xdr/vii-overlay.h"
#include "xdr/vii-transaction.h"
#include "xdr/vii-types.h"

namespace viichain
{

std::string xdr_printer(const PublicKey& pk);
}
