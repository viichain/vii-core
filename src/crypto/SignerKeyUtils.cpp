
#include "crypto/SignerKeyUtils.h"

#include "crypto/SHA.h"
#include "transactions/TransactionFrame.h"

namespace viichain
{

namespace SignerKeyUtils
{

SignerKey
preAuthTxKey(TransactionFrame const& tx)
{
    SignerKey sk;
    sk.type(SIGNER_KEY_TYPE_PRE_AUTH_TX);
    sk.preAuthTx() = tx.getContentsHash();
    return sk;
}

SignerKey
hashXKey(ByteSlice const& bs)
{
    SignerKey sk;
    sk.type(SIGNER_KEY_TYPE_HASH_X);
    sk.hashX() = sha256(bs);
    return sk;
}
}
}
