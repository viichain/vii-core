#pragma once


#include "crypto/ByteSlice.h"
#include "xdr/vii-types.h"
#include <memory>

namespace viichain
{

uint256 sha256(ByteSlice const& bin);

class SHA256
{
  public:
    static std::unique_ptr<SHA256> create();
    virtual ~SHA256(){};
    virtual void reset() = 0;
    virtual void add(ByteSlice const& bin) = 0;
    virtual uint256 finish() = 0;
};

HmacSha256Mac hmacSha256(HmacSha256Key const& key, ByteSlice const& bin);

bool hmacSha256Verify(HmacSha256Mac const& hmac, HmacSha256Key const& key,
                      ByteSlice const& bin);

HmacSha256Key hkdfExtract(ByteSlice const& bin);

HmacSha256Key hkdfExpand(HmacSha256Key const& key, ByteSlice const& bin);
}
