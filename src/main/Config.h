#pragma once

#include "crypto/SecretKey.h"
#include "lib/util/cpptoml.h"
#include "overlay/StellarXDR.h"
#include "util/SecretValue.h"
#include "util/Timer.h"
#include "util/optional.h"

#include <map>
#include <memory>
#include <string>

#define DEFAULT_PEER_PORT 11625

namespace viichain
{
struct HistoryArchiveConfiguration
{
    std::string mName;
    std::string mGetCmd;
    std::string mPutCmd;
    std::string mMkdirCmd;
};

class Config : public std::enable_shared_from_this<Config>
{
    enum class ValidatorQuality : int
    {
        VALIDATOR_LOW_QUALITY = 0,
        VALIDATOR_MED_QUALITY = 1,
        VALIDATOR_HIGH_QUALITY = 2
    };
    struct ValidatorEntry
    {
        std::string mName;
        std::string mHomeDomain;
        ValidatorQuality mQuality;
        PublicKey mKey;
        bool mHasHistory;
    };

    void validateConfig(bool mixed);
    void loadQset(std::shared_ptr<cpptoml::table> group, SCPQuorumSet& qset,
                  int level);

    void processConfig(std::shared_ptr<cpptoml::table>);

    void parseNodeID(std::string configStr, PublicKey& retKey);
    void parseNodeID(std::string configStr, PublicKey& retKey, SecretKey& sKey,
                     bool isSeed);

    std::string expandNodeID(std::string const& s) const;
    void addValidatorName(std::string const& pubKeyStr,
                          std::string const& name);
    void addHistoryArchive(std::string const& name, std::string const& get,
                           std::string const& put, std::string const& mkdir);

    std::string toString(ValidatorQuality q) const;
    ValidatorQuality parseQuality(std::string const& q) const;

    std::vector<ValidatorEntry>
    parseValidators(std::shared_ptr<cpptoml::base> validators,
                    std::unordered_map<std::string, ValidatorQuality> const&
                        domainQualityMap);

    std::unordered_map<std::string, ValidatorQuality>
    parseDomainsQuality(std::shared_ptr<cpptoml::base> domainsQuality);

    static SCPQuorumSet
    generateQuorumSetHelper(std::vector<ValidatorEntry>::const_iterator begin,
                            std::vector<ValidatorEntry>::const_iterator end,
                            ValidatorQuality curQuality);

    static SCPQuorumSet
    generateQuorumSet(std::vector<ValidatorEntry> const& validators);

    void
    addSelfToValidators(std::vector<ValidatorEntry>& validators,
                        std::unordered_map<std::string, ValidatorQuality> const&
                            domainQualityMap);

    void verifyHistoryValidatorsBlocking(
        std::vector<ValidatorEntry> const& validators);

  public:
    static const uint32 CURRENT_LEDGER_PROTOCOL_VERSION;

    typedef std::shared_ptr<Config> pointer;

    enum TestDbMode
    {
        TESTDB_DEFAULT,
        TESTDB_IN_MEMORY_SQLITE,
        TESTDB_ON_DISK_SQLITE,
#ifdef USE_POSTGRES
        TESTDB_POSTGRESQL,
#endif
        TESTDB_MODES
    };

    
                    
            bool FORCE_SCP;

            bool RUN_STANDALONE;

        bool MANUAL_CLOSE;

                bool CATCHUP_COMPLETE;

                        uint32_t CATCHUP_RECENT;

        std::chrono::seconds AUTOMATIC_MAINTENANCE_PERIOD;

            uint32_t AUTOMATIC_MAINTENANCE_COUNT;

                    bool ARTIFICIALLY_GENERATE_LOAD_FOR_TESTING;

                bool ARTIFICIALLY_ACCELERATE_TIME_FOR_TESTING;

            uint32 ARTIFICIALLY_SET_CLOSE_TIME_FOR_TESTING;

                    bool ARTIFICIALLY_PESSIMIZE_MERGES_FOR_TESTING;

                    bool ARTIFICIALLY_REDUCE_MERGE_COUNTS_FOR_TESTING;

            bool ALLOW_LOCALHOST_FOR_TESTING;

            bool USE_CONFIG_FOR_GENESIS;

                    int32_t FAILURE_SAFETY;

                    bool UNSAFE_QUORUM;

            bool DISABLE_BUCKET_GC;

        std::vector<std::string> KNOWN_CURSORS;

    uint32_t LEDGER_PROTOCOL_VERSION;
    VirtualClock::time_point TESTING_UPGRADE_DATETIME;

            uint64 MAXIMUM_LEDGER_CLOSETIME_DRIFT;

            uint32_t OVERLAY_PROTOCOL_MIN_VERSION; // min overlay version understood
    uint32_t OVERLAY_PROTOCOL_VERSION;     // max overlay version understood
    std::string VERSION_STR;
    std::string LOG_FILE_PATH;
    std::string BUCKET_DIR_PATH;
    uint32_t TESTING_UPGRADE_DESIRED_FEE; // in stroops
    uint32_t TESTING_UPGRADE_RESERVE;     // in stroops
    uint32_t TESTING_UPGRADE_MAX_TX_SET_SIZE;
    unsigned short HTTP_PORT; // what port to listen for commands
    bool PUBLIC_HTTP_PORT;    // if you accept commands from not localhost
    int HTTP_MAX_CLIENT;      // maximum number of http clients, i.e backlog
    std::string NETWORK_PASSPHRASE; // identifier for the network

        unsigned short PEER_PORT;
    unsigned short TARGET_PEER_CONNECTIONS;
    unsigned short MAX_PENDING_CONNECTIONS;
    int MAX_ADDITIONAL_PEER_CONNECTIONS;
    unsigned short MAX_INBOUND_PENDING_CONNECTIONS;
    unsigned short MAX_OUTBOUND_PENDING_CONNECTIONS;
    unsigned short PEER_AUTHENTICATION_TIMEOUT;
    unsigned short PEER_TIMEOUT;
    unsigned short PEER_STRAGGLER_TIMEOUT;
    static constexpr auto const POSSIBLY_PREFERRED_EXTRA = 2;
    static constexpr auto const REALLY_DEAD_NUM_FAILURES_CUTOFF = 120;

        std::vector<std::string> PREFERRED_PEERS;
    std::vector<std::string> KNOWN_PEERS;

        std::vector<std::string> PREFERRED_PEER_KEYS;

        bool PREFERRED_PEERS_ONLY;

                            uint32_t MINIMUM_IDLE_PERCENT;

        int WORKER_THREADS;

        int MAX_CONCURRENT_SUBPROCESSES;

        SecretKey NODE_SEED;
    bool NODE_IS_VALIDATOR;
    viichain::SCPQuorumSet QUORUM_SET;
        std::string NODE_HOME_DOMAIN;

        bool QUORUM_INTERSECTION_CHECKER;

        std::vector<std::string> INVARIANT_CHECKS;

    std::map<std::string, std::string> VALIDATOR_NAMES;

        std::map<std::string, HistoryArchiveConfiguration> HISTORY;

        SecretValue DATABASE;

    std::vector<std::string> COMMANDS;
    std::vector<std::string> REPORT_METRICS;

                            size_t ENTRY_CACHE_SIZE;
    size_t BEST_OFFERS_CACHE_SIZE;

                    size_t PREFETCH_BATCH_SIZE;

    Config();

    void load(std::string const& filename);
    void load(std::istream& in);

        void adjust();

    std::string toShortString(PublicKey const& pk) const;
    std::string toStrKey(PublicKey const& pk, bool& isAlias) const;
    std::string toStrKey(PublicKey const& pk) const;
    bool resolveNodeID(std::string const& s, PublicKey& retKey) const;

    std::chrono::seconds getExpectedLedgerCloseTime() const;

    void logBasicInfo();
    void setNoListen();

        std::string toString(SCPQuorumSet const& qset);
};
}
