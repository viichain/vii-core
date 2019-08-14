
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
    auto values = getVIIValues(envelope.statement);
    auto result = std::vector<Hash>{};
    result.resize(values.size());

    std::transform(std::begin(values), std::end(values), std::begin(result),
                   [](VIIValue const& sv) { return sv.txSetHash; });

    return result;
}

std::vector<VIIValue>
getVIIValues(SCPStatement const& statement)
{
    auto values = Slot::getStatementValues(statement);
    auto result = std::vector<VIIValue>{};
    result.resize(values.size());

    std::transform(std::begin(values), std::end(values), std::begin(result),
                   [](Value const& v) {
                       auto wb = VIIValue{};
                       xdr::xdr_from_opaque(v, wb);
                       return wb;
                   });

    return result;
}
}
