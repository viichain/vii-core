
#pragma once

#include "work/Work.h"

namespace viichain
{

struct StateSnapshot;

class ResolveSnapshotWork : public BasicWork
{
    std::shared_ptr<StateSnapshot> mSnapshot;
    std::unique_ptr<VirtualTimer> mTimer;
    asio::error_code mEc{asio::error_code()};

  public:
    ResolveSnapshotWork(Application& app,
                        std::shared_ptr<StateSnapshot> snapshot);
    ~ResolveSnapshotWork() = default;

  protected:
    State onRun() override;
    bool
    onAbort() override
    {
        return true;
    };
};
}
