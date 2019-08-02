
#include "historywork/GzipFileWork.h"
#include "util/Fs.h"

namespace viichain
{

GzipFileWork::GzipFileWork(Application& app, std::string const& filenameNoGz,
                           bool keepExisting)
    : RunCommandWork(app, std::string("gzip-file ") + filenameNoGz)
    , mFilenameNoGz(filenameNoGz)
    , mKeepExisting(keepExisting)
{
    fs::checkNoGzipSuffix(mFilenameNoGz);
}

void
GzipFileWork::onReset()
{
    std::string filenameGz = mFilenameNoGz + ".gz";
    std::remove(filenameGz.c_str());
}

CommandInfo
GzipFileWork::getCommand()
{
    std::string cmdLine = "gzip ";
    std::string outFile;
    if (mKeepExisting)
    {
        cmdLine += "-c ";
        outFile = mFilenameNoGz + ".gz";
    }
    cmdLine += mFilenameNoGz;

    return CommandInfo{cmdLine, outFile};
}
}
