#pragma once


#include "crypto/ByteSlice.h"
#include "xdr/vii-types.h"

namespace viichain
{

std::string binToHex(ByteSlice const& bin);

std::string hexAbbrev(ByteSlice const& bin);

std::vector<uint8_t> hexToBin(std::string const& hex);

uint256 hexToBin256(std::string const& encoded);
}
