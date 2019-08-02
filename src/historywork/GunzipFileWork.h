
#pragma once

#include "historywork/RunCommandWork.h"

namespace viichain
{

class GunzipFileWork : public RunCommandWork
{
    std::string const mFilenameGz;
    bool const mKeepExisting;
    CommandInfo getCommand() override;

  public:
    GunzipFileWork(Application& app, std::string const& filenameGz,
                   bool keepExisting = false,
                   size_t maxRetries = Work::RETRY_NEVER);
    ~GunzipFileWork() = default;

  protected:
    void onReset() override;
};
}
