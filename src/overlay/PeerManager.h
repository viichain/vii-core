#pragma once


#include "overlay/PeerBareAddress.h"
#include "util/Timer.h"

#include <functional>

namespace soci
{
class statement;
}

namespace viichain
{

class Database;
class RandomPeerSource;

enum class PeerType
{
    INBOUND,
    OUTBOUND,
    PREFERRED
};

enum class PeerTypeFilter
{
    INBOUND_ONLY,
    OUTBOUND_ONLY,
    PREFERRED_ONLY,
    ANY_OUTBOUND
};

struct PeerRecord
{
    std::tm mNextAttempt;
    int mNumFailures{0};
    int mType{0};
};

bool operator==(PeerRecord const& x, PeerRecord const& y);

struct PeerQuery
{
    bool mUseNextAttempt;
    int mMaxNumFailures;
    PeerTypeFilter mTypeFilter;
};

PeerAddress toXdr(PeerBareAddress const& address);

class PeerManager
{
  public:
    enum class TypeUpdate
    {
        SET_OUTBOUND,
        SET_PREFERRED,
        REMOVE_PREFERRED,
        UPDATE_TO_OUTBOUND
    };

    enum class BackOffUpdate
    {
        HARD_RESET,
        RESET,
        INCREASE
    };

    static void dropAll(Database& db);

    explicit PeerManager(Application& app);

        void ensureExists(PeerBareAddress const& address);

        void update(PeerBareAddress const& address, TypeUpdate type);

        void update(PeerBareAddress const& address, BackOffUpdate backOff);

        void update(PeerBareAddress const& address, TypeUpdate type,
                BackOffUpdate backOff);

        std::pair<PeerRecord, bool> load(PeerBareAddress const& address);

        void store(PeerBareAddress const& address, PeerRecord const& PeerRecord,
               bool inDatabase);

        std::vector<PeerBareAddress> loadRandomPeers(PeerQuery const& query,
                                                 int size);

        void removePeersWithManyFailures(int minNumFailures,
                                     PeerBareAddress const* address = nullptr);

        std::vector<PeerBareAddress> getPeersToSend(int size,
                                                PeerBareAddress const& address);

  private:
    static const char* kSQLCreateStatement;

    Application& mApp;
    std::unique_ptr<RandomPeerSource> mOutboundPeersToSend;
    std::unique_ptr<RandomPeerSource> mInboundPeersToSend;

    int countPeers(std::string const& where,
                   std::function<void(soci::statement&)> const& bind);
    std::vector<PeerBareAddress>
    loadPeers(int limit, int offset, std::string const& where,
              std::function<void(soci::statement&)> const& bind);

    void update(PeerRecord& peer, TypeUpdate type);
    void update(PeerRecord& peer, BackOffUpdate backOff, Application& app);
};
}
