#pragma once


#include "history/HistoryArchive.h"
#include "util/Timer.h"
#include "util/TmpDir.h"

#include <map>
#include <memory>
#include <vector>

namespace viichain
{

class FileTransferInfo;

struct StateSnapshot : public std::enable_shared_from_this<StateSnapshot>
{
    Application& mApp;
    HistoryArchiveState mLocalState;
    TmpDir mSnapDir;
    std::shared_ptr<FileTransferInfo> mLedgerSnapFile;
    std::shared_ptr<FileTransferInfo> mTransactionSnapFile;
    std::shared_ptr<FileTransferInfo> mTransactionResultSnapFile;
    std::shared_ptr<FileTransferInfo> mSCPHistorySnapFile;

    StateSnapshot(Application& app, HistoryArchiveState const& state);
    void makeLive();
    bool writeHistoryBlocks() const;
};
}
