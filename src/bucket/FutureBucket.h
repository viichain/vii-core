#pragma once


#include "overlay/StellarXDR.h"
#include <cereal/cereal.hpp>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace viichain
{

class Bucket;
class Application;

class FutureBucket
{
                            
    enum State
    {
        FB_CLEAR = 0,       // No inputs; no outputs; no hashes.
        FB_HASH_OUTPUT = 1, // Output hash; no output; no inputs or hashes.
        FB_HASH_INPUTS = 2, // Input hashes; no inputs; no outputs or hashes.
        FB_LIVE_OUTPUT = 3, // Output; output hashes; _maybe_ inputs and hashes.
        FB_LIVE_INPUTS = 4, // Inputs; input hashes; no outputs. Merge running.
    };

    State mState{FB_CLEAR};

                        std::shared_ptr<Bucket> mInputCurrBucket;
    std::shared_ptr<Bucket> mInputSnapBucket;
    std::vector<std::shared_ptr<Bucket>> mInputShadowBuckets;
    std::shared_future<std::shared_ptr<Bucket>> mOutputBucket;

                        std::string mInputCurrBucketHash;
    std::string mInputSnapBucketHash;
    std::vector<std::string> mInputShadowBucketHashes;
    std::string mOutputBucketHash;

    void checkHashesMatch() const;
    void checkState() const;
    void startMerge(Application& app, uint32_t maxProtocolVersion,
                    bool keepDeadEntries, bool countMergeEvents);

    void clearInputs();
    void clearOutput();
    void setLiveOutput(std::shared_ptr<Bucket> b);

  public:
    FutureBucket(Application& app, std::shared_ptr<Bucket> const& curr,
                 std::shared_ptr<Bucket> const& snap,
                 std::vector<std::shared_ptr<Bucket>> const& shadows,
                 uint32_t maxProtocolVersion, bool keepDeadEntries,
                 bool countMergeEvents);

    FutureBucket() = default;
    FutureBucket(FutureBucket const& other) = default;
    FutureBucket& operator=(FutureBucket const& other) = default;

        void clear();

        bool isLive() const;

            bool isMerging() const;

        bool hasHashes() const;

        bool hasOutputHash() const;

        std::string const& getOutputHash() const;

        bool mergeComplete() const;

        std::shared_ptr<Bucket> resolve();

        void makeLive(Application& app, uint32_t maxProtocolVersion,
                  bool keepDeadEntries);

        std::vector<std::string> getHashes() const;

    template <class Archive>
    void
    load(Archive& ar)
    {
        clear();
        ar(cereal::make_nvp("state", mState));
        switch (mState)
        {
        case FB_HASH_INPUTS:
            ar(cereal::make_nvp("curr", mInputCurrBucketHash));
            ar(cereal::make_nvp("snap", mInputSnapBucketHash));
            ar(cereal::make_nvp("shadow", mInputShadowBucketHashes));
            break;
        case FB_HASH_OUTPUT:
            ar(cereal::make_nvp("output", mOutputBucketHash));
            break;
        case FB_CLEAR:
            break;
        default:
            throw std::runtime_error(
                "deserialized unexpected FutureBucket state");
            break;
        }
        checkState();
    }

    template <class Archive>
    void
    save(Archive& ar) const
    {
        checkState();
        switch (mState)
        {
        case FB_LIVE_INPUTS:
        case FB_HASH_INPUTS:
            ar(cereal::make_nvp("state", FB_HASH_INPUTS));
            ar(cereal::make_nvp("curr", mInputCurrBucketHash));
            ar(cereal::make_nvp("snap", mInputSnapBucketHash));
            ar(cereal::make_nvp("shadow", mInputShadowBucketHashes));
            break;
        case FB_LIVE_OUTPUT:
        case FB_HASH_OUTPUT:
            ar(cereal::make_nvp("state", FB_HASH_OUTPUT));
            ar(cereal::make_nvp("output", mOutputBucketHash));
            break;
        case FB_CLEAR:
            ar(cereal::make_nvp("state", FB_CLEAR));
            break;
        default:
            assert(false);
            break;
        }
    }
};
}
