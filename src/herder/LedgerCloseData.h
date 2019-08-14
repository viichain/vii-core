#pragma once


#include "TxSetFrame.h"
#include "main/Config.h"
#include "overlay/VIIXDR.h"
#include <string>

namespace viichain
{

class LedgerCloseData
{
  public:
    LedgerCloseData(uint32_t ledgerSeq, TxSetFramePtr txSet,
                    VIIValue const& v);

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
    VIIValue const&
    getValue() const
    {
        return mValue;
    }

  private:
    uint32_t mLedgerSeq;
    TxSetFramePtr mTxSet;
    VIIValue mValue;
};

std::string viiValueToString(Config const& c, VIIValue const& sv);

#define emptyUpgradeSteps (xdr::xvector<UpgradeType, 6>(0))
}
