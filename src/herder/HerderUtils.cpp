
#include "herder/HerderUtils.h"
#include "scp/Slot.h"
#include "xdr/vii-ledger.h"
#include <algorithm>
#include <xdrpp/marshal.h>

namespace viichain
{

std::vector<Hash>
getTxSetHashes(SCPEnvelope const& envelope)
{
    auto values = getStellarValues(envelope.statement);
    auto result = std::vector<Hash>{};
    result.resize(values.size());

    std::transform(std::begin(values), std::end(values), std::begin(result),
                   [](StellarValue const& sv) { return sv.txSetHash; });

    return result;
}

std::vector<StellarValue>
getStellarValues(SCPStatement const& statement)
{
    auto values = Slot::getStatementValues(statement);
    auto result = std::vector<StellarValue>{};
    result.resize(values.size());

    std::transform(std::begin(values), std::end(values), std::begin(result),
                   [](Value const& v) {
                       auto wb = StellarValue{};
                       xdr::xdr_from_opaque(v, wb);
                       return wb;
                   });

    return result;
}
}
