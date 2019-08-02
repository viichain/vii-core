
#pragma once

#include "work/Work.h"

namespace viichain
{

struct StateSnapshot;

class WriteSnapshotWork : public BasicWork
{
    std::shared_ptr<StateSnapshot> mSnapshot;
    bool mDone{false};
    bool mSuccess{true};

  public:
    WriteSnapshotWork(Application& app,
                      std::shared_ptr<StateSnapshot> snapshot);
    ~WriteSnapshotWork() = default;

  protected:
    State onRun() override;
    bool
    onAbort() override
    {
        return true;
    };
};
}
