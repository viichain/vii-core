#pragma once


#include "history/HistoryArchive.h"
#include "history/InferredQuorum.h"
#include "overlay/StellarXDR.h"
#include <functional>
#include <memory>

namespace asio
{
typedef std::error_code error_code;
};

namespace viichain
{
class Application;
class Bucket;
class BucketList;
class Config;
class Database;
class HistoryArchive;
struct StateSnapshot;

class HistoryManager
{
  public:
            enum LedgerVerificationStatus
    {
        VERIFY_STATUS_OK,
        VERIFY_STATUS_ERR_BAD_HASH,
        VERIFY_STATUS_ERR_BAD_LEDGER_VERSION,
        VERIFY_STATUS_ERR_OVERSHOT,
        VERIFY_STATUS_ERR_UNDERSHOT,
        VERIFY_STATUS_ERR_MISSING_ENTRIES
    };

        static bool checkSensibleConfig(Config const& cfg);

    static std::unique_ptr<HistoryManager> create(Application& app);

        static void dropAll(Database& db);

                virtual uint32_t getCheckpointFrequency() const = 0;

                virtual uint32_t checkpointContainingLedger(uint32_t ledger) const = 0;

                        virtual uint32_t prevCheckpointLedger(uint32_t ledger) const = 0;

                                virtual uint32_t nextCheckpointLedger(uint32_t ledger) const = 0;

            virtual void logAndUpdatePublishStatus() = 0;

        virtual size_t publishQueueLength() const = 0;

                    virtual bool maybeQueueHistoryCheckpoint() = 0;

                    virtual void queueCurrentHistory() = 0;

            virtual uint32_t getMinLedgerQueuedToPublish() = 0;

            virtual uint32_t getMaxLedgerQueuedToPublish() = 0;

            virtual size_t publishQueuedHistory() = 0;

                virtual std::vector<std::string>
    getMissingBucketsReferencedByPublishQueue() = 0;

            virtual std::vector<std::string> getBucketsReferencedByPublishQueue() = 0;

                        virtual void
    historyPublished(uint32_t ledgerSeq,
                     std::vector<std::string> const& originalBuckets,
                     bool success) = 0;

    virtual void downloadMissingBuckets(
        HistoryArchiveState desiredState,
        std::function<void(asio::error_code const& ec)> handler) = 0;

        virtual HistoryArchiveState getLastClosedHistoryArchiveState() const = 0;

        virtual InferredQuorum inferQuorum(uint32_t ledgerNum) = 0;

            virtual std::string const& getTmpDir() = 0;

            virtual std::string localFilename(std::string const& basename) = 0;

                virtual uint64_t getPublishQueueCount() = 0;

        virtual uint64_t getPublishSuccessCount() = 0;

        virtual uint64_t getPublishFailureCount() = 0;

    virtual ~HistoryManager(){};
};
}
