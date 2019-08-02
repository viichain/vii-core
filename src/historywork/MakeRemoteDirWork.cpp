
#include "historywork/MakeRemoteDirWork.h"
#include "history/HistoryArchive.h"
#include "main/Application.h"

namespace viichain
{

MakeRemoteDirWork::MakeRemoteDirWork(Application& app, std::string const& dir,
                                     std::shared_ptr<HistoryArchive> archive)
    : RunCommandWork(app, std::string("make-remote-dir ") + dir)
    , mDir(dir)
    , mArchive(archive)
{
    assert(mArchive);
}

CommandInfo
MakeRemoteDirWork::getCommand()
{
    std::string cmdLine;
    if (mArchive->hasMkdirCmd())
    {
        cmdLine = mArchive->mkdirCmd(mDir);
    }
    return CommandInfo{cmdLine, std::string()};
}

void
MakeRemoteDirWork::onSuccess()
{
    mArchive->markSuccess();
}

void
MakeRemoteDirWork::onFailureRaise()
{
    mArchive->markFailure();
    RunCommandWork::onFailureRaise();
}
}
