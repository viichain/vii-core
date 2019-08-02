
#pragma once

#include "work/Work.h"

namespace medida
{
class Meter;
}

namespace viichain
{

class HistoryArchive;
struct HistoryArchiveState;

class GetHistoryArchiveStateWork : public Work
{
    std::shared_ptr<BasicWork> mGetRemoteFile;

    HistoryArchiveState& mState;
    uint32_t mSeq;
    std::shared_ptr<HistoryArchive> mArchive;
    size_t mRetries;
    std::string mLocalFilename;

    medida::Meter& mGetHistoryArchiveStateSuccess;

  public:
    GetHistoryArchiveStateWork(
        Application& app, HistoryArchiveState& state, uint32_t seq = 0,
        std::shared_ptr<HistoryArchive> archive = nullptr,
        size_t maxRetries = BasicWork::RETRY_A_FEW);
    ~GetHistoryArchiveStateWork() = default;

  protected:
    BasicWork::State doWork() override;
    void doReset() override;
    void onSuccess() override;
};
}
