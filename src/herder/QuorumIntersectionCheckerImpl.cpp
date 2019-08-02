
#include "QuorumIntersectionCheckerImpl.h"
#include "QuorumIntersectionChecker.h"

#include "util/Logging.h"
#include "util/Math.h"

namespace
{


using namespace viichain;
struct QBitSet;
using QGraph = std::vector<QBitSet>;

QBitSet::QBitSet(uint32_t threshold, BitSet const& nodes,
                 QGraph const& innerSets)
    : mThreshold(threshold)
    , mNodes(nodes)
    , mInnerSets(innerSets)
    , mAllSuccessors(getSuccessors(nodes, innerSets))
{
}

void
QBitSet::log(size_t indent) const
{
    std::string s(indent, ' ');
    CLOG(DEBUG, "SCP") << s << "QBitSet: thresh=" << mThreshold << "/"
                       << (mNodes.count() + mInnerSets.size())
                       << " validators=" << mNodes;
    for (auto const& inner : mInnerSets)
    {
        inner.log(indent + 4);
    }
}

BitSet
QBitSet::getSuccessors(BitSet const& nodes, QGraph const& inner)
{
    BitSet out(nodes);
    for (auto const& i : inner)
    {
        out |= i.mAllSuccessors;
    }
    return out;
}


TarjanSCCCalculator::TarjanSCCCalculator(QGraph const& graph) : mGraph(graph)
{
}

void
TarjanSCCCalculator::calculateSCCs()
{
    mNodes.clear();
    mStack.clear();
    mIndex = 0;
    mSCCs.clear();
    for (size_t i = 0; i < mGraph.size(); ++i)
    {
        mNodes.emplace_back(SCCNode{});
    }
    for (size_t i = 0; i < mGraph.size(); ++i)
    {
        if (mNodes.at(i).mIndex == -1)
        {
            scc(i);
        }
    }
}

void
TarjanSCCCalculator::scc(size_t i)
{
    auto& v = mNodes.at(i);
    v.mIndex = mIndex;
    v.mLowLink = mIndex;
    mIndex++;
    mStack.push_back(i);
    v.mOnStack = true;

    BitSet const& succ = mGraph.at(i).mAllSuccessors;
    for (size_t j = 0; succ.nextSet(j); ++j)
    {
        CLOG(TRACE, "SCP") << "edge: " << i << " -> " << j;
        SCCNode& w = mNodes.at(j);
        if (w.mIndex == -1)
        {
            scc(j);
            v.mLowLink = std::min(v.mLowLink, w.mLowLink);
        }
        else if (w.mOnStack)
        {
            v.mLowLink = std::min(v.mLowLink, w.mIndex);
        }
    }

    if (v.mLowLink == v.mIndex)
    {
        BitSet newScc;
        newScc.set(i);
        size_t j = 0;
        do
        {
            j = mStack.back();
            newScc.set(j);
            mStack.pop_back();
            mNodes.at(j).mOnStack = false;
        } while (j != i);
        mSCCs.push_back(newScc);
    }
}


size_t
MinQuorumEnumerator::pickSplitNode() const
{
    std::vector<size_t> inDegrees(mQic.mGraph.size(), 0);
    assert(!mRemaining.empty());
    size_t maxNode = mRemaining.max();
    size_t maxCount = 1;
    size_t maxDegree = 0;
    for (size_t i = 0; mRemaining.nextSet(i); ++i)
    {

                                BitSet avail = mQic.mGraph.at(i).mAllSuccessors & mRemaining;
        for (size_t j = 0; avail.nextSet(j); ++j)
        {
            size_t currDegree = ++inDegrees.at(j);
            if (currDegree >= maxDegree)
            {
                if (currDegree == maxDegree)
                {
                                                            maxCount++;
                    if (rand_uniform<size_t>(0, maxCount) == 0)
                    {
                                                continue;
                    }
                                    }
                else
                {
                                        maxCount = 1;
                }
                maxDegree = currDegree;
                maxNode = j;
            }
        }
    }
    return maxNode;
}

size_t
MinQuorumEnumerator::maxCommit() const
{
    return (mQic.mMaxSCC.count() / 2) + 1;
}

MinQuorumEnumerator::MinQuorumEnumerator(
    BitSet const& committed, BitSet const& remaining,
    QuorumIntersectionCheckerImpl const& qic)
    : mCommitted(committed)
    , mRemaining(remaining)
    , mPerimeter(committed | remaining)
    , mQic(qic)
{
}

bool
MinQuorumEnumerator::anyMinQuorumHasDisjointQuorum()
{
    mQic.mStats.mCallsStarted++;

        if ((mQic.mStats.mCallsStarted & 0xfffff) == 0)
    {
        mQic.mStats.log();
    }
    if (mQic.mLogTrace)
    {
        CLOG(TRACE, "SCP") << "exploring with committed=" << mCommitted;
        CLOG(TRACE, "SCP") << "exploring with remaining=" << mRemaining;
    }

                    if (mCommitted.count() > maxCommit())
    {
        mQic.mStats.mEarlyExit1s++;
        if (mQic.mLogTrace)
        {
            CLOG(TRACE, "SCP") << "early exit 1, with committed=" << mCommitted;
        }
        return false;
    }

                        if (mQic.mLogTrace)
    {
        CLOG(TRACE, "SCP") << "checking for quorum in committed=" << mCommitted;
    }
    if (auto committedQuorum = mQic.contractToMaximalQuorum(mCommitted))
    {
        if (mQic.isMinimalQuorum(committedQuorum))
        {
                                    if (mQic.mLogTrace)
            {
                CLOG(TRACE, "SCP")
                    << "early exit 3.1: minimal quorum=" << committedQuorum;
            }
            mQic.mStats.mEarlyExit31s++;
            return mQic.hasDisjointQuorum(committedQuorum);
        }
        if (mQic.mLogTrace)
        {
            CLOG(TRACE, "SCP")
                << "early exit 3.2: non-minimal quorum=" << committedQuorum;
        }
        mQic.mStats.mEarlyExit32s++;
        return false;
    }

                if (mQic.mLogTrace)
    {
        CLOG(TRACE, "SCP") << "checking for quorum in perimeter=" << mPerimeter;
    }
    if (auto extensionQuorum = mQic.contractToMaximalQuorum(mPerimeter))
    {
        if (!(mCommitted <= extensionQuorum))
        {
            if (mQic.mLogTrace)
            {
                CLOG(TRACE, "SCP")
                    << "early exit 2.2: extension quorum=" << extensionQuorum
                    << " in perimeter=" << mPerimeter
                    << " does not extend committed=" << mCommitted;
            }
            mQic.mStats.mEarlyExit22s++;
            return false;
        }
    }
    else
    {
        if (mQic.mLogTrace)
        {
            CLOG(TRACE, "SCP")
                << "early exit 2.1: no extension quorum in perimeter="
                << mPerimeter;
        }
        mQic.mStats.mEarlyExit21s++;
        return false;
    }

        if (!mRemaining)
    {
        mQic.mStats.mTerminations++;
        if (mQic.mLogTrace)
        {
            CLOG(TRACE, "SCP") << "remainder exhausted";
        }
        return false;
    }

        size_t split = pickSplitNode();
    if (mQic.mLogTrace)
    {
        CLOG(TRACE, "SCP") << "recursing into subproblems, split=" << split;
    }
    mRemaining.unset(split);
    MinQuorumEnumerator childExcludingSplit(mCommitted, mRemaining, mQic);
    mQic.mStats.mFirstRecursionsTaken++;
    if (childExcludingSplit.anyMinQuorumHasDisjointQuorum())
    {
        if (mQic.mLogTrace)
        {
            CLOG(TRACE, "SCP")
                << "first subproblem returned true, missing split=" << split;
        }
        return true;
    }
    mCommitted.set(split);
    MinQuorumEnumerator childIncludingSplit(mCommitted, mRemaining, mQic);
    mQic.mStats.mSecondRecursionsTaken++;
    return childIncludingSplit.anyMinQuorumHasDisjointQuorum();
}


QuorumIntersectionCheckerImpl::QuorumIntersectionCheckerImpl(
    QuorumTracker::QuorumMap const& qmap, Config const& cfg)
    : mCfg(cfg), mLogTrace(Logging::logTrace("SCP")), mTSC(mGraph)
{
    buildGraph(qmap);
    buildSCCs();
}

std::pair<std::vector<PublicKey>, std::vector<PublicKey>>
QuorumIntersectionCheckerImpl::getPotentialSplit() const
{
    return mPotentialSplit;
}

size_t
QuorumIntersectionCheckerImpl::getMaxQuorumsFound() const
{
    return mStats.mMaxQuorumsSeen;
}

void
QuorumIntersectionCheckerImpl::Stats::log() const
{
    CLOG(DEBUG, "SCP") << "Quorum intersection checker stats:";
    size_t exits = (mEarlyExit1s + mEarlyExit21s + mEarlyExit22s +
                    mEarlyExit31s + mEarlyExit32s);
    CLOG(DEBUG, "SCP") << "[Nodes: " << mTotalNodes << ", SCCs: " << mNumSCCs
                       << ", MaxSCC: " << mMaxSCC
                       << ", MaxQs:" << mMaxQuorumsSeen
                       << ", MinQs:" << mMinQuorumsSeen
                       << ", Calls:" << mCallsStarted
                       << ", Terms:" << mTerminations << ", Exits:" << exits
                       << "]";
    CLOG(DEBUG, "SCP") << "Detailed exit stats:";
    CLOG(DEBUG, "SCP") << "[X1:" << mEarlyExit1s << ", X2.1:" << mEarlyExit21s
                       << ", X2.2:" << mEarlyExit22s
                       << ", X3.1:" << mEarlyExit31s
                       << ", X3.2:" << mEarlyExit32s << "]";
}

bool
QuorumIntersectionCheckerImpl::containsQuorumSlice(BitSet const& bs,
                                                   QBitSet const& qbs) const
{
                size_t intersecting = bs.intersectionCount(qbs.mNodes);
    if (intersecting >= qbs.mThreshold)
    {
        return true;
    }

                size_t innerThreshold = qbs.mThreshold - intersecting;
    if (innerThreshold > qbs.mInnerSets.size())
    {
        return false;
    }

                                if (bs.intersectionCount(qbs.mAllSuccessors) < qbs.mThreshold)
    {
        return false;
    }

                                    size_t innerFailLimit = qbs.mInnerSets.size() - innerThreshold + 1;
    for (auto const& inner : qbs.mInnerSets)
    {
        if (containsQuorumSlice(bs, inner))
        {
            innerThreshold--;
            if (innerThreshold == 0)
            {
                return true;
            }
        }
        else
        {
            innerFailLimit--;
            if (innerFailLimit == 0)
            {
                return false;
            }
        }
    }
    return false;
}

bool
QuorumIntersectionCheckerImpl::containsQuorumSliceForNode(BitSet const& bs,
                                                          size_t node) const
{
    if (!bs.get(node))
    {
        return false;
    }
    return containsQuorumSlice(bs, mGraph.at(node));
}

bool
QuorumIntersectionCheckerImpl::isAQuorum(BitSet const& nodes) const
{
    return (bool)contractToMaximalQuorum(nodes);
}

BitSet
QuorumIntersectionCheckerImpl::contractToMaximalQuorum(BitSet nodes) const
{
            if (mLogTrace)
    {
        CLOG(TRACE, "SCP") << "Contracting to max quorum of " << nodes;
    }
    while (true)
    {
        BitSet filtered(nodes.count());
        for (size_t i = 0; nodes.nextSet(i); ++i)
        {
            if (containsQuorumSliceForNode(nodes, i))
            {
                if (mLogTrace)
                {
                    CLOG(TRACE, "SCP") << "Have qslice for " << i;
                }
                filtered.set(i);
            }
            else
            {
                if (mLogTrace)
                {
                    CLOG(TRACE, "SCP") << "Missing qslice for " << i;
                }
            }
        }
        if (filtered.count() == nodes.count() || filtered.empty())
        {
            if (mLogTrace)
            {
                CLOG(TRACE, "SCP") << "Contracted to max quorum " << filtered;
            }
            if (filtered)
            {
                ++mStats.mMaxQuorumsSeen;
            }
            return filtered;
        }
        nodes = filtered;
    }
}

bool
QuorumIntersectionCheckerImpl::isMinimalQuorum(BitSet const& nodes) const
{
#ifndef NDEBUG
            assert(contractToMaximalQuorum(nodes) == nodes);
#endif

    BitSet minQ = nodes;
    if (!nodes)
    {
                return false;
    }
    for (size_t i = 0; nodes.nextSet(i); ++i)
    {
        minQ.unset(i);
        if (isAQuorum(minQ))
        {
                        return false;
        }
                minQ.set(i);
    }
            mStats.mMinQuorumsSeen++;
    return true;
}

void
QuorumIntersectionCheckerImpl::noteFoundDisjointQuorums(
    BitSet const& nodes, BitSet const& disj) const
{
    mPotentialSplit.first.clear();
    mPotentialSplit.second.clear();

            CLOG(DEBUG, "SCP") << "Disjoint quorum IDs: " << nodes << " vs. " << disj;

    std::ostringstream err;
    err << "Found potential disjoint quorums: ";
    nodes.streamWith(err, [this](std::ostream& out, size_t i) {
        out << this->nodeName(i);
        this->mPotentialSplit.first.emplace_back(this->mBitNumPubKeys.at(i));
    });
    err << " vs. ";
    disj.streamWith(err, [this](std::ostream& out, size_t i) {
        out << this->nodeName(i);
        this->mPotentialSplit.second.emplace_back(this->mBitNumPubKeys.at(i));
    });
    CLOG(ERROR, "SCP") << err.str();
}

bool
QuorumIntersectionCheckerImpl::hasDisjointQuorum(BitSet const& nodes) const
{
    BitSet disj = contractToMaximalQuorum(mMaxSCC - nodes);
    if (disj)
    {
        noteFoundDisjointQuorums(nodes, disj);
    }
    else
    {
        if (mLogTrace)
        {
            CLOG(TRACE, "SCP")
                << "no quorum in complement  = " << (mMaxSCC - nodes);
        }
    }
    return disj;
}

QBitSet
QuorumIntersectionCheckerImpl::convertSCPQuorumSet(SCPQuorumSet const& sqs)
{
    uint32_t threshold = sqs.threshold;
    BitSet nodeBits(mPubKeyBitNums.size());
    for (auto const& v : sqs.validators)
    {
        auto i = mPubKeyBitNums.find(v);
        if (i == mPubKeyBitNums.end())
        {
                                                                                                                                                                                                                                                                                                            CLOG(DEBUG, "SCP") << "Depending on node with missing QSet: "
                               << mCfg.toShortString(v);
        }
        else
        {
            nodeBits.set(i->second);
        }
    }
    QGraph inner;
    inner.reserve(sqs.innerSets.size());
    for (auto const& i : sqs.innerSets)
    {
        inner.emplace_back(convertSCPQuorumSet(i));
    }
    return QBitSet(threshold, nodeBits, inner);
}

void
QuorumIntersectionCheckerImpl::buildGraph(QuorumTracker::QuorumMap const& qmap)
{
    mPubKeyBitNums.clear();
    mBitNumPubKeys.clear();
    mGraph.clear();

    for (auto const& pair : qmap)
    {
        if (pair.second)
        {
            size_t n = mBitNumPubKeys.size();
            mPubKeyBitNums.insert(std::make_pair(pair.first, n));
            mBitNumPubKeys.emplace_back(pair.first);
        }
        else
        {
            CLOG(DEBUG, "SCP")
                << "Node with missing QSet: " << mCfg.toShortString(pair.first);
        }
    }

    for (auto const& pair : qmap)
    {
        if (pair.second)
        {
            auto i = mPubKeyBitNums.find(pair.first);
            assert(i != mPubKeyBitNums.end());
            auto nodeNum = i->second;
            assert(nodeNum == mGraph.size());
            auto qb = convertSCPQuorumSet(*pair.second);
            qb.log();
            mGraph.emplace_back(qb);
        }
    }
    mStats.mTotalNodes = mPubKeyBitNums.size();
}

void
QuorumIntersectionCheckerImpl::buildSCCs()
{
    mTSC.calculateSCCs();
    mMaxSCC.clear();
    for (auto const& scc : mTSC.mSCCs)
    {
        if (scc.count() > mMaxSCC.count())
        {
            mMaxSCC = scc;
        }
        CLOG(DEBUG, "SCP") << "Found " << scc.count() << "-node SCC " << scc;
    }
    CLOG(DEBUG, "SCP") << "Maximal SCC is " << mMaxSCC;
    mStats.mNumSCCs = mTSC.mSCCs.size();
    mStats.mMaxSCC = mMaxSCC.count();
}

std::string
QuorumIntersectionCheckerImpl::nodeName(size_t node) const
{
    return mCfg.toShortString(mBitNumPubKeys.at(node));
}

bool
QuorumIntersectionCheckerImpl::networkEnjoysQuorumIntersection() const
{
            bool foundDisjoint = false;
    size_t nNodes = mPubKeyBitNums.size();
    CLOG(INFO, "SCP") << "Calculating " << nNodes
                      << "-node network quorum intersection";

    for (auto const& scc : mTSC.mSCCs)
    {
        if (scc == mMaxSCC)
        {
            continue;
        }
        if (auto other = contractToMaximalQuorum(scc))
        {
            CLOG(DEBUG, "SCP") << "found SCC-disjoint quorum = " << other;
            CLOG(DEBUG, "SCP") << "disjoint from quorum = "
                               << contractToMaximalQuorum(mMaxSCC);
            noteFoundDisjointQuorums(contractToMaximalQuorum(mMaxSCC), other);
            foundDisjoint = true;
            break;
        }
        else
        {
            CLOG(DEBUG, "SCP") << "SCC contains no quorums = " << scc;
            for (size_t i = 0; scc.nextSet(i); ++i)
            {
                CLOG(DEBUG, "SCP") << "Node outside main SCC: " << nodeName(i);
            }
        }
    }
    for (size_t i = 0; mMaxSCC.nextSet(i); ++i)
    {
        CLOG(DEBUG, "SCP") << "Main SCC node: " << nodeName(i);
    }

    auto q = contractToMaximalQuorum(mMaxSCC);
    if (q)
    {
        CLOG(DEBUG, "SCP") << "Maximal main SCC quorum: " << q;
    }
    else
    {
                                CLOG(WARNING, "SCP")
            << "No quorum found in transitive closure (possible network halt)";
        return true;
    }

        if (!foundDisjoint)
    {
        BitSet committed;
        BitSet remaining = mMaxSCC;
        MinQuorumEnumerator mqe(committed, remaining, *this);
        foundDisjoint = mqe.anyMinQuorumHasDisjointQuorum();
        mStats.log();
    }
    return !foundDisjoint;
}
}

namespace viichain
{
std::shared_ptr<QuorumIntersectionChecker>
QuorumIntersectionChecker::create(QuorumTracker::QuorumMap const& qmap,
                                  Config const& cfg)
{
    return std::make_shared<QuorumIntersectionCheckerImpl>(qmap, cfg);
}
}
