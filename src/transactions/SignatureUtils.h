#pragma once


#include "xdr/vii-types.h"

namespace viichain
{

class ByteSlice;
class SecretKey;
struct DecoratedSignature;
struct SignerKey;

namespace SignatureUtils
{

DecoratedSignature sign(SecretKey const& secretKey, Hash const& hash);
bool verify(DecoratedSignature const& sig, SignerKey const& signerKey,
            Hash const& hash);

DecoratedSignature signHashX(const ByteSlice& x);
bool verifyHashX(DecoratedSignature const& sig, SignerKey const& signerKey);

SignatureHint getHint(ByteSlice const& bs);
bool doesHintMatch(ByteSlice const& bs, SignatureHint const& hint);
}
}
