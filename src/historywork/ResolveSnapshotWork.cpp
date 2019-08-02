
#include "historywork/ResolveSnapshotWork.h"
#include "history/StateSnapshot.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"

namespace viichain
{

ResolveSnapshotWork::ResolveSnapshotWork(
    Application& app, std::shared_ptr<StateSnapshot> snapshot)
    : BasicWork(app, "prepare-snapshot", Work::RETRY_NEVER)
    , mSnapshot(snapshot)
    , mTimer(std::make_unique<VirtualTimer>(app.getClock()))
{
}

BasicWork::State
ResolveSnapshotWork::onRun()
{
    if (mEc)
    {
        return State::WORK_FAILURE;
    }

    mSnapshot->mLocalState.resolveAnyReadyFutures();
    mSnapshot->makeLive();
    if ((mApp.getLedgerManager().getLastClosedLedgerNum() >
         mSnapshot->mLocalState.currentLedger) &&
        mSnapshot->mLocalState.futuresAllResolved())
    {
        return State::WORK_SUCCESS;
    }
    else
    {
        std::weak_ptr<ResolveSnapshotWork> weak(
            std::static_pointer_cast<ResolveSnapshotWork>(shared_from_this()));
        auto handler = [weak](asio::error_code const& ec) {
            auto self = weak.lock();
            if (self)
            {
                self->mEc = ec;
                self->wakeUp();
            }
        };
        mTimer->expires_from_now(std::chrono::seconds(1));
        mTimer->async_wait(handler);
        return State::WORK_WAITING;
    }
}
}
