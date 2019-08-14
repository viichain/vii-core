#pragma once


#include "overlay/VIIXDR.h"
#include "util/XDROperators.h"

namespace viichain
{
struct LedgerEntryIdCmp
{
    template <typename T, typename U>
    auto
    operator()(T const& a, U const& b) const
        -> decltype(a.type(), b.type(), bool())
    {
        LedgerEntryType aty = a.type();
        LedgerEntryType bty = b.type();

        if (aty < bty)
            return true;

        if (aty > bty)
            return false;

        switch (aty)
        {

        case ACCOUNT:
            return a.account().accountID < b.account().accountID;

        case TRUSTLINE:
        {
            auto const& atl = a.trustLine();
            auto const& btl = b.trustLine();
            if (atl.accountID < btl.accountID)
                return true;
            if (btl.accountID < atl.accountID)
                return false;
            {
                return atl.asset < btl.asset;
            }
        }

        case OFFER:
        {
            auto const& aof = a.offer();
            auto const& bof = b.offer();
            if (aof.sellerID < bof.sellerID)
                return true;
            if (bof.sellerID < aof.sellerID)
                return false;
            return aof.offerID < bof.offerID;
        }
        case DATA:
        {
            auto const& ad = a.data();
            auto const& bd = b.data();
            if (ad.accountID < bd.accountID)
                return true;
            if (bd.accountID < ad.accountID)
                return false;
            {
                return ad.dataName < bd.dataName;
            }
        }
        }
        return false;
    }
};

struct BucketEntryIdCmp
{
    bool
    operator()(BucketEntry const& a, BucketEntry const& b) const
    {
        BucketEntryType aty = a.type();
        BucketEntryType bty = b.type();

                if (aty == METAENTRY || bty == METAENTRY)
        {
            return aty < bty;
        }

        if (aty == LIVEENTRY || aty == INITENTRY)
        {
            if (bty == LIVEENTRY || bty == INITENTRY)
            {
                return LedgerEntryIdCmp{}(a.liveEntry().data,
                                          b.liveEntry().data);
            }
            else
            {
                if (bty != DEADENTRY)
                {
                    throw std::runtime_error("Malformed bucket: unexpected "
                                             "non-INIT/LIVE/DEAD entry.");
                }
                return LedgerEntryIdCmp{}(a.liveEntry().data, b.deadEntry());
            }
        }
        else
        {
            if (aty != DEADENTRY)
            {
                throw std::runtime_error(
                    "Malformed bucket: unexpected non-INIT/LIVE/DEAD entry.");
            }
            if (bty == LIVEENTRY || bty == INITENTRY)
            {
                return LedgerEntryIdCmp{}(a.deadEntry(), b.liveEntry().data);
            }
            else
            {
                if (bty != DEADENTRY)
                {
                    throw std::runtime_error("Malformed bucket: unexpected "
                                             "non-INIT/LIVE/DEAD entry.");
                }
                return LedgerEntryIdCmp{}(a.deadEntry(), b.deadEntry());
            }
        }
    }
};
}
