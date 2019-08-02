#pragma once


#include "TxSetFrame.h"
#include "main/Config.h"
#include "overlay/StellarXDR.h"
#include <string>

namespace viichain
{

class LedgerCloseData
{
  public:
    LedgerCloseData(uint32_t ledgerSeq, TxSetFramePtr txSet,
                    StellarValue const& v);

    uint32_t
    getLedgerSeq() const
    {
        return mLedgerSeq;
    }
    TxSetFramePtr
    getTxSet() const
    {
        return mTxSet;
    }
    StellarValue const&
    getValue() const
    {
        return mValue;
    }

  private:
    uint32_t mLedgerSeq;
    TxSetFramePtr mTxSet;
    StellarValue mValue;
};

std::string viiValueToString(Config const& c, StellarValue const& sv);

#define emptyUpgradeSteps (xdr::xvector<UpgradeType, 6>(0))
}
