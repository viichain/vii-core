
#pragma once

#include "history/HistoryManager.h"
#include "ledger/LedgerRange.h"
#include "work/Work.h"

namespace medida
{
class Meter;
}

namespace viichain
{

class TmpDir;
struct LedgerHeaderHistoryEntry;

class VerifyLedgerChainWork : public BasicWork
{
    TmpDir const& mDownloadDir;
    LedgerRange const mRange;
    uint32_t mCurrCheckpoint;
    LedgerNumHashPair const& mLastClosed;
    LedgerNumHashPair const mTrustedEndLedger;

                LedgerNumHashPair mVerifiedAhead;

        LedgerHeaderHistoryEntry mVerifiedLedgerRangeStart{};

    medida::Meter& mVerifyLedgerSuccess;
    medida::Meter& mVerifyLedgerChainSuccess;
    medida::Meter& mVerifyLedgerChainFailure;

    HistoryManager::LedgerVerificationStatus verifyHistoryOfSingleCheckpoint();

  public:
    VerifyLedgerChainWork(Application& app, TmpDir const& downloadDir,
                          LedgerRange range,
                          LedgerNumHashPair const& lastClosedLedger,
                          LedgerNumHashPair ledgerRangeEnd);
    ~VerifyLedgerChainWork() override = default;
    std::string getStatus() const override;

    LedgerHeaderHistoryEntry
    getVerifiedLedgerRangeStart()
    {
        return mVerifiedLedgerRangeStart;
    }

  protected:
    void onReset() override;

    BasicWork::State onRun() override;
    bool
    onAbort() override
    {
        return true;
    };
};
}
