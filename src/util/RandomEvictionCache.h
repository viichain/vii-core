#pragma once

#include "util/Math.h"
#include "util/NonCopyable.h"

#include <random>
#include <unordered_map>

namespace viichain
{

template <typename K, typename V>
class RandomEvictionCache : public NonMovableOrCopyable
{
  public:
    struct Counters
    {
        uint64_t mHits{0};
        uint64_t mMisses{0};
        uint64_t mInserts{0};
        uint64_t mUpdates{0};
        uint64_t mEvicts{0};
    };

  private:
        size_t mMaxSize;

            uint64_t mGeneration{0};
    struct CacheValue
    {
        uint64_t mLastAccess;
        V mValue;
    };

        std::unordered_map<K, CacheValue> mValueMap;
    using MapValueType = typename std::unordered_map<K, CacheValue>::value_type;

                    std::vector<MapValueType*> mValuePtrs;

        Counters mCounters;

        void
    evictOne()
    {
        size_t sz = mValuePtrs.size();
        if (sz == 0)
        {
            return;
        }
        MapValueType*& vp1 = mValuePtrs.at(rand_uniform<size_t>(0, sz - 1));
        MapValueType*& vp2 = mValuePtrs.at(rand_uniform<size_t>(0, sz - 1));
        MapValueType*& victim =
            (vp1->second.mLastAccess < vp2->second.mLastAccess ? vp1 : vp2);
        mValueMap.erase(victim->first);
        std::swap(victim, mValuePtrs.back());
        mValuePtrs.pop_back();
        ++mCounters.mEvicts;
    }

  public:
    explicit RandomEvictionCache(size_t maxSize) : mMaxSize(maxSize)
    {
        mValueMap.reserve(maxSize + 1);
        mValuePtrs.reserve(maxSize + 1);
    }

    size_t
    maxSize() const
    {
        return mMaxSize;
    }

    size_t
    size() const
    {
        return mValueMap.size();
    }

    Counters const&
    getCounters() const
    {
        return mCounters;
    }

                void
    put(K const& k, V const& v)
    {
        ++mGeneration;
        CacheValue newValue{mGeneration, v};
        auto pair = mValueMap.insert(std::make_pair(k, newValue));
        if (pair.second)
        {
                                                            MapValueType& inserted = *pair.first;
            mValuePtrs.push_back(&inserted);
            ++mCounters.mInserts;
                        if (mValuePtrs.size() > mMaxSize)
            {
                evictOne();
            }
        }
        else
        {
                        CacheValue& existing = pair.first->second;
            existing = newValue;
            ++mCounters.mUpdates;
        }
    }

        bool
    exists(K const& k, bool countMisses = true)
    {
        bool miss = (mValueMap.find(k) == mValueMap.end());
                                                        if (miss && countMisses)
        {
            ++mCounters.mMisses;
        }
        return !miss;
    }

        void
    clear()
    {
        mValuePtrs.clear();
        mValueMap.clear();
    }

            void
    erase_if(std::function<bool(V const&)> const& f)
    {
        for (size_t i = 0; i < mValuePtrs.size(); ++i)
        {
            MapValueType*& vp = mValuePtrs.at(i);
            while (mValuePtrs.size() != i && f(vp->second.mValue))
            {
                                                                                                mValueMap.erase(vp->first);
                std::swap(vp, mValuePtrs.back());
                mValuePtrs.pop_back();
            }
        }
    }

        V&
    get(K const& k)
    {
        auto it = mValueMap.find(k);
        if (it != mValueMap.end())
        {
            auto& cacheVal = it->second;
            ++mCounters.mHits;
            cacheVal.mLastAccess = ++mGeneration;
            return cacheVal.mValue;
        }
        else
        {
            throw std::range_error("There is no such key in cache");
        }
    }
};
}
