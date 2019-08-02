#pragma once


#include "overlay/BanManager.h"

namespace viichain
{

class BanManagerImpl : public BanManager
{
  protected:
    Application& mApp;

  public:
    BanManagerImpl(Application& app);
    ~BanManagerImpl();

    void banNode(NodeID nodeID) override;
    void unbanNode(NodeID nodeID) override;
    bool isBanned(NodeID nodeID) override;
    std::vector<std::string> getBans() override;
};
}
