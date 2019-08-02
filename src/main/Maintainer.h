#pragma once


#include "util/Timer.h"

#include <cstdint>

namespace viichain
{

class Application;

class Maintainer
{
  public:
    explicit Maintainer(Application& app);

        void start();

        void performMaintenance(uint32_t count);

  private:
    Application& mApp;
    VirtualTimer mTimer;

    void scheduleMaintenance();
    void tick();
};
}
