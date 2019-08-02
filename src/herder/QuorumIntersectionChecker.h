#pragma once


#include "herder/QuorumTracker.h"
#include <memory>

namespace viichain
{

class Config;

class QuorumIntersectionChecker
{
  public:
    static std::shared_ptr<QuorumIntersectionChecker>
    create(viichain::QuorumTracker::QuorumMap const& qmap,
           viichain::Config const& cfg);

    virtual ~QuorumIntersectionChecker(){};
    virtual bool networkEnjoysQuorumIntersection() const = 0;
    virtual size_t getMaxQuorumsFound() const = 0;
    virtual std::pair<std::vector<PublicKey>, std::vector<PublicKey>>
    getPotentialSplit() const = 0;
};
}
