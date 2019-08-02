#pragma once


#include "herder/HerderPersistence.h"

namespace viichain
{
class Application;

class HerderPersistenceImpl : public HerderPersistence
{

  public:
    HerderPersistenceImpl(Application& app);
    ~HerderPersistenceImpl();

    void saveSCPHistory(uint32_t seq, std::vector<SCPEnvelope> const& envs,
                        QuorumTracker::QuorumMap const& qmap) override;

  private:
    Application& mApp;
};
}
