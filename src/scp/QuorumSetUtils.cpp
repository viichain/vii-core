
#include "QuorumSetUtils.h"

#include "util/XDROperators.h"
#include "xdr/vii-SCP.h"
#include "xdr/vii-types.h"

#include <algorithm>
#include <set>

namespace viichain
{

namespace
{

class QuorumSetSanityChecker
{
  public:
    explicit QuorumSetSanityChecker(SCPQuorumSet const& qSet, bool extraChecks);
    bool
    isSane() const
    {
        return mIsSane;
    }

  private:
    bool mExtraChecks;
    std::set<NodeID> mKnownNodes;
    bool mIsSane;
    size_t mCount{0};

    bool checkSanity(SCPQuorumSet const& qSet, int depth);
};

QuorumSetSanityChecker::QuorumSetSanityChecker(SCPQuorumSet const& qSet,
                                               bool extraChecks)
    : mExtraChecks{extraChecks}
{
    mIsSane = checkSanity(qSet, 0) && mCount >= 1 && mCount <= 1000;
}

bool
QuorumSetSanityChecker::checkSanity(SCPQuorumSet const& qSet, int depth)
{
    if (depth > 2)
        return false;

    if (qSet.threshold < 1)
        return false;

    auto& v = qSet.validators;
    auto& i = qSet.innerSets;

    size_t totEntries = v.size() + i.size();
    size_t vBlockingSize = totEntries - qSet.threshold + 1;
    mCount += v.size();

    if (qSet.threshold > totEntries)
        return false;

        if (mExtraChecks && qSet.threshold < vBlockingSize)
        return false;

    for (auto const& n : v)
    {
        auto r = mKnownNodes.insert(n);
        if (!r.second)
        {
                        return false;
        }
    }

    for (auto const& iSet : i)
    {
        if (!checkSanity(iSet, depth + 1))
        {
            return false;
        }
    }

    return true;
}
}

bool
isQuorumSetSane(SCPQuorumSet const& qSet, bool extraChecks)
{
    QuorumSetSanityChecker checker{qSet, extraChecks};
    return checker.isSane();
}

namespace
{

void
normalizeQSetSimplify(SCPQuorumSet& qSet, NodeID const* idToRemove)
{
    using xdr::operator==;
    auto& v = qSet.validators;
    if (idToRemove)
    {
        auto it_v = std::remove_if(v.begin(), v.end(), [&](NodeID const& n) {
            return n == *idToRemove;
        });
        qSet.threshold -= uint32(v.end() - it_v);
        v.erase(it_v, v.end());
    }

    auto& i = qSet.innerSets;
    auto it = i.begin();
    while (it != i.end())
    {
        normalizeQSetSimplify(*it, idToRemove);
                if (it->threshold == 1 && it->validators.size() == 1 &&
            it->innerSets.size() == 0)
        {
            v.emplace_back(it->validators.front());
            it = i.erase(it);
        }
        else
        {
            it++;
        }
    }

        if (qSet.threshold == 1 && v.size() == 0 && i.size() == 1)
    {
        auto t = qSet.innerSets.back();
        qSet = t;
    }
}

template <typename InputIt1, typename InputIt2, class Compare>
int
intLexicographicalCompare(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                          InputIt2 last2, Compare comp)
{
    for (; first1 != last1 && first2 != last2; first1++, first2++)
    {
        auto c = comp(*first1, *first2);
        if (c != 0)
        {
            return c;
        }
    }
    if (first1 == last1 && first2 != last2)
    {
        return -1;
    }
    if (first1 != last1 && first2 == last2)
    {
        return 1;
    }
    return 0;
}

int
qSetCompareInt(SCPQuorumSet const& l, SCPQuorumSet const& r)
{
    auto& lvals = l.validators;
    auto& rvals = r.validators;

        auto res = intLexicographicalCompare(
        lvals.begin(), lvals.end(), rvals.begin(), rvals.end(),
        [](PublicKey const& l, PublicKey const& r) {
            if (l < r)
            {
                return -1;
            }
            if (r < l)
            {
                return 1;
            }
            return 0;
        });
    if (res != 0)
    {
        return res;
    }

        auto const& li = l.innerSets;
    auto const& ri = r.innerSets;
    res = intLexicographicalCompare(li.begin(), li.end(), ri.begin(), ri.end(),
                                    qSetCompareInt);
    if (res != 0)
    {
        return res;
    }

        return (l.threshold < r.threshold) ? -1
                                       : ((l.threshold == r.threshold) ? 0 : 1);
}

void
normalizeQuorumSetReorder(SCPQuorumSet& qset)
{
    std::sort(qset.validators.begin(), qset.validators.end());
    for (auto& qs : qset.innerSets)
    {
        normalizeQuorumSetReorder(qs);
    }
        std::sort(qset.innerSets.begin(), qset.innerSets.end(),
              [](SCPQuorumSet const& l, SCPQuorumSet const& r) {
                  return qSetCompareInt(l, r) < 0;
              });
}
}
void
normalizeQSet(SCPQuorumSet& qSet, NodeID const* idToRemove)
{
    normalizeQSetSimplify(qSet, idToRemove);
    normalizeQuorumSetReorder(qSet);
}
}
