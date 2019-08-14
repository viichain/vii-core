﻿#include "util/asio.h"
#include "LedgerCloseData.h"
#include "crypto/Hex.h"
#include "herder/Upgrades.h"
#include "main/Application.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include <overlay/OverlayManager.h>
#include <xdrpp/marshal.h>

using namespace std;

namespace viichain
{

LedgerCloseData::LedgerCloseData(uint32_t ledgerSeq, TxSetFramePtr txSet,
                                 VIIValue const& v)
    : mLedgerSeq(ledgerSeq), mTxSet(txSet), mValue(v)
{
    Value x;
    Value y(x.begin(), x.end());

    assert(txSet->getContentsHash() == mValue.txSetHash);
}

std::string
viiValueToString(Config const& c, VIIValue const& sv)
{
    std::stringstream res;

    res << "[";
    if (sv.ext.v() == VII_VALUE_SIGNED)
    {
        res << " SIGNED@" << c.toShortString(sv.ext.lcValueSignature().nodeID);
    }
    res << " txH: " << hexAbbrev(sv.txSetHash) << ", ct: " << sv.closeTime
        << ", upgrades: [";
    for (auto const& upgrade : sv.upgrades)
    {
        if (upgrade.empty())
        {
                        res << "<empty>";
        }
        else
        {
            try
            {
                LedgerUpgrade lupgrade;
                xdr::xdr_from_opaque(upgrade, lupgrade);
                res << Upgrades::toString(lupgrade);
            }
            catch (std::exception&)
            {
                res << "<unknown>";
            }
        }
        res << ", ";
    }
    res << " ] ]";

    return res.str();
}
}
