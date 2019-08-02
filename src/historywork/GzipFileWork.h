
#pragma once

#include "historywork/RunCommandWork.h"

namespace viichain
{

class GzipFileWork : public RunCommandWork
{
    std::string const mFilenameNoGz;
    bool const mKeepExisting;
    CommandInfo getCommand() override;

  public:
    GzipFileWork(Application& app, std::string const& filenameNoGz,
                 bool keepExisting = false);
    ~GzipFileWork() = default;

  protected:
    void onReset() override;
};
}
