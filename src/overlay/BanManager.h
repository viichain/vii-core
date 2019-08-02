#pragma once


#include "xdr/vii-types.h"

namespace viichain
{

class Application;
class Database;

class BanManager
{
  public:
    static std::unique_ptr<BanManager> create(Application& app);
    static void dropAll(Database& db);

        virtual void banNode(NodeID nodeID) = 0;

        virtual void unbanNode(NodeID nodeID) = 0;

        virtual bool isBanned(NodeID nodeID) = 0;

        virtual std::vector<std::string> getBans() = 0;

    virtual ~BanManager()
    {
    }
};
}
