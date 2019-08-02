
#include "historywork/PutRemoteFileWork.h"
#include "history/HistoryArchive.h"
#include "main/Application.h"

namespace viichain
{
PutRemoteFileWork::PutRemoteFileWork(Application& app, std::string const& local,
                                     std::string const& remote,
                                     std::shared_ptr<HistoryArchive> archive)
    : RunCommandWork(app, std::string("put-remote-file ") + remote)
    , mLocal(local)
    , mRemote(remote)
    , mArchive(archive)
{
    assert(mArchive);
    assert(mArchive->hasPutCmd());
}

CommandInfo
PutRemoteFileWork::getCommand()
{
    auto cmdLine = mArchive->putFileCmd(mLocal, mRemote);
    return CommandInfo{cmdLine, std::string()};
}

void
PutRemoteFileWork::onSuccess()
{
    mArchive->markSuccess();
    RunCommandWork::onSuccess();
}

void
PutRemoteFileWork::onFailureRaise()
{
    mArchive->markFailure();
    RunCommandWork::onFailureRaise();
}
}
