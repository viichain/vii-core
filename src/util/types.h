#pragma once


#include "lib/util/uint128_t.h"
#include "numeric.h"
#include "overlay/StellarXDR.h"
#include "xdrpp/message.h"
#include <type_traits>
#include <vector>

namespace viichain
{
typedef std::vector<unsigned char> Blob;

LedgerKey LedgerEntryKey(LedgerEntry const& e);

bool isZero(uint256 const& b);

Hash& operator^=(Hash& l, Hash const& r);

bool lessThanXored(Hash const& l, Hash const& r, Hash const& x);

bool isString32Valid(std::string const& str);

bool isAssetValid(Asset const& cur);

AccountID getIssuer(Asset const& asset);

bool compareAsset(Asset const& first, Asset const& second);

int32_t unsignedToSigned(uint32_t v);

int64_t unsignedToSigned(uint64_t v);

std::string formatSize(size_t size);

template <uint32_t N>
void
assetCodeToStr(xdr::opaque_array<N> const& code, std::string& retStr)
{
    retStr.clear();
    for (auto c : code)
    {
        if (!c)
        {
            break;
        }
        retStr.push_back(c);
    }
};

template <uint32_t N>
void
strToAssetCode(xdr::opaque_array<N>& ret, std::string const& str)
{
    ret.fill(0);
    size_t n = std::min(ret.size(), str.size());
    std::copy(str.begin(), str.begin() + n, ret.begin());
}

bool addBalance(int64_t& balance, int64_t delta,
                int64_t maxBalance = std::numeric_limits<int64_t>::max());

bool iequals(std::string const& a, std::string const& b);

bool operator>=(Price const& a, Price const& b);
bool operator>(Price const& a, Price const& b);
bool operator==(Price const& a, Price const& b);
}
