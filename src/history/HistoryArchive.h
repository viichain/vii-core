#pragma once


#include "bucket/FutureBucket.h"
#include "main/Config.h"
#include "xdr/vii-types.h"

#include <cereal/cereal.hpp>
#include <memory>
#include <string>
#include <system_error>

namespace asio
{
typedef std::error_code error_code;
}

namespace medida
{
class Meter;
}

namespace viichain
{

class Application;
class BucketList;
class Bucket;

struct HistoryStateBucket
{
    std::string curr;
    FutureBucket next;
    std::string snap;

    template <class Archive>
    void
    serialize(Archive& ar) const
    {
        ar(CEREAL_NVP(curr), CEREAL_NVP(next), CEREAL_NVP(snap));
    }

    template <class Archive>
    void
    serialize(Archive& ar)
    {
        ar(CEREAL_NVP(curr), CEREAL_NVP(next), CEREAL_NVP(snap));
    }
};

struct HistoryArchiveState
{
    static unsigned const HISTORY_ARCHIVE_STATE_VERSION;

    unsigned version{HISTORY_ARCHIVE_STATE_VERSION};
    std::string server;
    uint32_t currentLedger{0};
    std::vector<HistoryStateBucket> currentBuckets;

    HistoryArchiveState();

    HistoryArchiveState(uint32_t ledgerSeq, BucketList const& buckets);

    static std::string baseName();
    static std::string wellKnownRemoteDir();
    static std::string wellKnownRemoteName();
    static std::string remoteDir(uint32_t snapshotNumber);
    static std::string remoteName(uint32_t snapshotNumber);
    static std::string localName(Application& app,
                                 std::string const& archiveName);

        Hash getBucketListHash();

                    std::vector<std::string>
    differingBuckets(HistoryArchiveState const& other) const;

        std::vector<std::string> allBuckets() const;

    template <class Archive>
    void
    serialize(Archive& ar)
    {
        ar(CEREAL_NVP(version), CEREAL_NVP(server), CEREAL_NVP(currentLedger),
           CEREAL_NVP(currentBuckets));
    }

    template <class Archive>
    void
    serialize(Archive& ar) const
    {
        ar(CEREAL_NVP(version), CEREAL_NVP(server), CEREAL_NVP(currentLedger),
           CEREAL_NVP(currentBuckets));
    }

                bool futuresAllReady() const;

        bool futuresAllResolved() const;

            void resolveAllFutures();

                    void resolveAnyReadyFutures();

    void save(std::string const& outFile) const;
    void load(std::string const& inFile);

    std::string toString() const;
    void fromString(std::string const& str);
};

class HistoryArchive : public std::enable_shared_from_this<HistoryArchive>
{
  public:
    explicit HistoryArchive(Application& app,
                            HistoryArchiveConfiguration const& config);
    ~HistoryArchive();
    bool hasGetCmd() const;
    bool hasPutCmd() const;
    bool hasMkdirCmd() const;
    std::string const& getName() const;

    std::string getFileCmd(std::string const& remote,
                           std::string const& local) const;
    std::string putFileCmd(std::string const& local,
                           std::string const& remote) const;
    std::string mkdirCmd(std::string const& remoteDir) const;

    void markSuccess();
    void markFailure();

    uint64_t getSuccessCount() const;
    uint64_t getFailureCount() const;

  private:
    HistoryArchiveConfiguration mConfig;
    medida::Meter& mSuccessMeter;
    medida::Meter& mFailureMeter;
};
}
