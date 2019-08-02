#pragma once


#include "catchup/CatchupManager.h"
#include <memory>

namespace medida
{
class Meter;
}

namespace viichain
{

class Application;
class Work;

class CatchupManagerImpl : public CatchupManager
{
    Application& mApp;
    std::shared_ptr<BasicWork> mCatchupWork;

  public:
    CatchupManagerImpl(Application& app);
    ~CatchupManagerImpl() override;

    void historyCaughtup() override;

    void catchupHistory(CatchupConfiguration catchupConfiguration,
                        CatchupWork::ProgressHandler handler) override;

    std::string getStatus() const override;

    void logAndUpdateCatchupStatus(bool contiguous,
                                   std::string const& message) override;
    void logAndUpdateCatchupStatus(bool contiguous) override;
};
}
