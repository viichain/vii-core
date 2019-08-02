
#include "historywork/PublishWork.h"
#include "history/HistoryArchiveManager.h"
#include "history/HistoryManager.h"
#include "history/StateSnapshot.h"
#include "lib/util/format.h"
#include "main/Application.h"
#include "util/Logging.h"

namespace viichain
{

PublishWork::PublishWork(Application& app,
                         std::shared_ptr<StateSnapshot> snapshot,
                         std::vector<std::shared_ptr<BasicWork>> seq)
    : WorkSequence(
          app,
          fmt::format("publish-{:08x}", snapshot->mLocalState.currentLedger),
          seq)
    , mSnapshot(snapshot)
    , mOriginalBuckets(mSnapshot->mLocalState.allBuckets())
{
}

void
PublishWork::onFailureRaise()
{
            mApp.getHistoryManager().historyPublished(
        mSnapshot->mLocalState.currentLedger, mOriginalBuckets, false);
}

void
PublishWork::onSuccess()
{
            mApp.getHistoryManager().historyPublished(
        mSnapshot->mLocalState.currentLedger, mOriginalBuckets, true);
}
}
