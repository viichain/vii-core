#pragma once


namespace viichain
{

class ByteSlice;
class TransactionFrame;
struct SignerKey;

namespace SignerKeyUtils
{

SignerKey preAuthTxKey(TransactionFrame const& tx);
SignerKey hashXKey(ByteSlice const& bs);
}
}
