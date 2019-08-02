#pragma once

#include "ByteSlice.h"
#include <string>

namespace viichain
{

struct SecretValue;

namespace strKey
{

enum StrKeyVersionByte : uint8_t
{
        STRKEY_PUBKEY_ED25519 = 21, // 'V'
    STRKEY_SEED_ED25519 = 8,    // 'I'
    STRKEY_PRE_AUTH_TX = 19,    // 'T',
    STRKEY_HASH_X = 23          // 'X'
};

SecretValue toStrKey(uint8_t ver, ByteSlice const& bin);

size_t getStrKeySize(size_t dataSize);

bool fromStrKey(std::string const& strKey, uint8_t& outVersion,
                std::vector<uint8_t>& decoded);
}
}
