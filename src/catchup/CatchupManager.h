#pragma once


#include "catchup/CatchupWork.h"
#include <functional>
#include <memory>
#include <system_error>

namespace asio
{

typedef std::error_code error_code;
};

namespace viichain
{

class Application;

class CatchupManager
{
  public:
    static std::unique_ptr<CatchupManager> create(Application& app);

        virtual void historyCaughtup() = 0;

        virtual void catchupHistory(CatchupConfiguration catchupConfiguration,
                                CatchupWork::ProgressHandler handler) = 0;

        virtual std::string getStatus() const = 0;

                        virtual void logAndUpdateCatchupStatus(bool contiguous,
                                           std::string const& message) = 0;

                        virtual void logAndUpdateCatchupStatus(bool contiguous) = 0;

    virtual ~CatchupManager(){};
};
}
