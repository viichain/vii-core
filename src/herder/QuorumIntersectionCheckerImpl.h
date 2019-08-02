#pragma once



#include "QuorumIntersectionChecker.h"
#include "main/Config.h"
#include "util/BitSet.h"
#include "xdr/vii-SCP.h"
#include "xdr/vii-types.h"

namespace
{

struct QBitSet;
using QGraph = std::vector<QBitSet>;
class QuorumIntersectionCheckerImpl;

struct QBitSet
{
    const uint32_t mThreshold;
    const BitSet mNodes;
    const QGraph mInnerSets;

            const BitSet mAllSuccessors;

    QBitSet(uint32_t threshold, BitSet const& nodes, QGraph const& innerSets);

    bool
    empty() const
    {
        return mThreshold == 0 && mAllSuccessors.empty();
    }

    void log(size_t indent = 0) const;

    static BitSet getSuccessors(BitSet const& nodes, QGraph const& inner);
};

struct TarjanSCCCalculator
{
    struct SCCNode
    {
        int mIndex = {-1};
        int mLowLink = {-1};
        bool mOnStack = {false};
    };

    std::vector<SCCNode> mNodes;
    std::vector<size_t> mStack;
    int mIndex = {0};
    std::vector<BitSet> mSCCs;
    QGraph const& mGraph;

    TarjanSCCCalculator(QGraph const& graph);
    void calculateSCCs();
    void scc(size_t i);
};

class MinQuorumEnumerator
{

                        BitSet mCommitted;

                    BitSet mRemaining;

            BitSet mPerimeter;

        QuorumIntersectionCheckerImpl const& mQic;

        size_t pickSplitNode() const;

        size_t maxCommit() const;

  public:
    MinQuorumEnumerator(BitSet const& committed, BitSet const& remaining,
                        QuorumIntersectionCheckerImpl const& qic);

    bool anyMinQuorumHasDisjointQuorum();
};

class QuorumIntersectionCheckerImpl : public viichain::QuorumIntersectionChecker
{

    viichain::Config const& mCfg;

    struct Stats
    {
        size_t mTotalNodes = {0};
        size_t mNumSCCs = {0};
        size_t mMaxSCC = {0};
        size_t mCallsStarted = {0};
        size_t mFirstRecursionsTaken = {0};
        size_t mSecondRecursionsTaken = {0};
        size_t mMaxQuorumsSeen = {0};
        size_t mMinQuorumsSeen = {0};
        size_t mTerminations = {0};
        size_t mEarlyExit1s = {0};
        size_t mEarlyExit21s = {0};
        size_t mEarlyExit22s = {0};
        size_t mEarlyExit31s = {0};
        size_t mEarlyExit32s = {0};
        void log() const;
    };

                mutable Stats mStats;
    bool mLogTrace;

            mutable std::pair<std::vector<viichain::PublicKey>,
                      std::vector<viichain::PublicKey>>
        mPotentialSplit;

            std::vector<viichain::PublicKey> mBitNumPubKeys;
    std::unordered_map<viichain::PublicKey, size_t> mPubKeyBitNums;
    QGraph mGraph;

            TarjanSCCCalculator mTSC;
    BitSet mMaxSCC;

    QBitSet convertSCPQuorumSet(viichain::SCPQuorumSet const& sqs);
    void buildGraph(viichain::QuorumTracker::QuorumMap const& qmap);
    void buildSCCs();

    bool containsQuorumSlice(BitSet const& bs, QBitSet const& qbs) const;
    bool containsQuorumSliceForNode(BitSet const& bs, size_t node) const;
    BitSet contractToMaximalQuorum(BitSet nodes) const;
    bool isAQuorum(BitSet const& nodes) const;
    bool isMinimalQuorum(BitSet const& nodes) const;
    bool hasDisjointQuorum(BitSet const& nodes) const;
    void noteFoundDisjointQuorums(BitSet const& nodes,
                                  BitSet const& disj) const;
    std::string nodeName(size_t node) const;

    friend class MinQuorumEnumerator;

  public:
    QuorumIntersectionCheckerImpl(viichain::QuorumTracker::QuorumMap const& qmap,
                                  viichain::Config const& cfg);
    bool networkEnjoysQuorumIntersection() const override;

    std::pair<std::vector<viichain::PublicKey>, std::vector<viichain::PublicKey>>
    getPotentialSplit() const override;
    size_t getMaxQuorumsFound() const override;
};
}
