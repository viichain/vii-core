
#pragma once

#include "herder/TxSetFrame.h"
#include "ledger/LedgerRange.h"
#include "util/XDRStream.h"
#include "work/Work.h"
#include "xdr/vii-SCP.h"
#include "xdr/vii-ledger.h"

namespace medida
{
class Meter;
}

namespace viichain
{

class TmpDir;
struct LedgerHeaderHistoryEntry;

class ApplyLedgerChainWork : public BasicWork
{
    TmpDir const& mDownloadDir;
    LedgerRange const mRange;
    uint32_t mCurrSeq;
    XDRInputFileStream mHdrIn;
    XDRInputFileStream mTxIn;
    TransactionHistoryEntry mTxHistoryEntry;
    LedgerHeaderHistoryEntry& mLastApplied;

    medida::Meter& mApplyLedgerSuccess;
    medida::Meter& mApplyLedgerFailure;

    bool mFilesOpen{false};

    TxSetFramePtr getCurrentTxSet();
    void openCurrentInputFiles();
    bool applyHistoryOfSingleLedger();

  public:
    ApplyLedgerChainWork(Application& app, TmpDir const& downloadDir,
                         LedgerRange range,
                         LedgerHeaderHistoryEntry& lastApplied);
    ~ApplyLedgerChainWork() = default;
    std::string getStatus() const override;

  protected:
    void onReset() override;
    State onRun() override;
    bool
    onAbort() override
    {
        return true;
    };
};
}
