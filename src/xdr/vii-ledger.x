
%#include "xdr/vii-SCP.h"
%#include "xdr/vii-transaction.h"

namespace viichain
{

typedef opaque UpgradeType<128>;

enum VIIValueType
{
    VII_VALUE_BASIC = 0,
    VII_VALUE_SIGNED = 1
};

struct LedgerCloseValueSignature
{
    NodeID nodeID;       // which node introduced the value
    Signature signature; // nodeID's signature
};

struct VIIValue
{
    Hash txSetHash;      // transaction set to apply to previous ledger
    TimePoint closeTime; // network close time

                        UpgradeType upgrades<6>;

        union switch (VIIValueType v)
    {
    case VII_VALUE_BASIC:
        void;
    case VII_VALUE_SIGNED:
        LedgerCloseValueSignature lcValueSignature;
    }
    ext;
};

struct LedgerHeader
{
    uint32 ledgerVersion;    // the protocol version of the ledger
    Hash previousLedgerHash; // hash of the previous ledger header
    VIIValue scpValue;   // what consensus agreed to
    Hash txSetResultHash;    // the TransactionResultSet that led to this ledger
    Hash bucketListHash;     // hash of the ledger state

    uint32 ledgerSeq; // sequence number of this ledger

    int64 totalCoins; // total number of stroops in existence.
                      
    int64 feePool;       // fees burned since last inflation run
    uint32 inflationSeq; // inflation sequence number

    uint64 idPool; // last used global ID, used for generating objects

    uint32 baseFee;     // base fee per operation in stroops
    uint32 baseReserve; // account base reserve in stroops

    uint32 maxTxSetSize; // maximum size a transaction set can be

    Hash skipList[4]; // hashes of ledgers in the past. allows you to jump back
                                                                                        
        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

enum LedgerUpgradeType
{
    LEDGER_UPGRADE_VERSION = 1,
    LEDGER_UPGRADE_BASE_FEE = 2,
    LEDGER_UPGRADE_MAX_TX_SET_SIZE = 3,
    LEDGER_UPGRADE_BASE_RESERVE = 4
};

union LedgerUpgrade switch (LedgerUpgradeType type)
{
case LEDGER_UPGRADE_VERSION:
    uint32 newLedgerVersion; // update ledgerVersion
case LEDGER_UPGRADE_BASE_FEE:
    uint32 newBaseFee; // update baseFee
case LEDGER_UPGRADE_MAX_TX_SET_SIZE:
    uint32 newMaxTxSetSize; // update maxTxSetSize
case LEDGER_UPGRADE_BASE_RESERVE:
    uint32 newBaseReserve; // update baseReserve
};

union LedgerKey switch (LedgerEntryType type)
{
case ACCOUNT:
    struct
    {
        AccountID accountID;
    } account;

case TRUSTLINE:
    struct
    {
        AccountID accountID;
        Asset asset;
    } trustLine;

case OFFER:
    struct
    {
        AccountID sellerID;
        int64 offerID;
    } offer;

case DATA:
    struct
    {
        AccountID accountID;
        string64 dataName;
    } data;
};

enum BucketEntryType
{
    METAENTRY =
        -1, // At-and-after protocol 11: bucket metadata, should come first.
    LIVEENTRY = 0, // Before protocol 11: created-or-updated;
                       DEADENTRY = 1,
    INITENTRY = 2 // At-and-after protocol 11: only created.
};

struct BucketMetadata
{
        uint32 ledgerVersion;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

union BucketEntry switch (BucketEntryType type)
{
case LIVEENTRY:
case INITENTRY:
    LedgerEntry liveEntry;

case DEADENTRY:
    LedgerKey deadEntry;
case METAENTRY:
    BucketMetadata metaEntry;
};

struct TransactionSet
{
    Hash previousLedgerHash;
    TransactionEnvelope txs<>;
};

struct TransactionResultPair
{
    Hash transactionHash;
    TransactionResult result; // result for the transaction
};

struct TransactionResultSet
{
    TransactionResultPair results<>;
};


struct TransactionHistoryEntry
{
    uint32 ledgerSeq;
    TransactionSet txSet;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

struct TransactionHistoryResultEntry
{
    uint32 ledgerSeq;
    TransactionResultSet txResultSet;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

struct LedgerHeaderHistoryEntry
{
    Hash hash;
    LedgerHeader header;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};


struct LedgerSCPMessages
{
    uint32 ledgerSeq;
    SCPEnvelope messages<>;
};

struct SCPHistoryEntryV0
{
    SCPQuorumSet quorumSets<>; // additional quorum sets used by ledgerMessages
    LedgerSCPMessages ledgerMessages;
};

union SCPHistoryEntry switch (int v)
{
case 0:
    SCPHistoryEntryV0 v0;
};



enum LedgerEntryChangeType
{
    LEDGER_ENTRY_CREATED = 0, // entry was added to the ledger
    LEDGER_ENTRY_UPDATED = 1, // entry was modified in the ledger
    LEDGER_ENTRY_REMOVED = 2, // entry was removed from the ledger
    LEDGER_ENTRY_STATE = 3    // value of the entry
};

union LedgerEntryChange switch (LedgerEntryChangeType type)
{
case LEDGER_ENTRY_CREATED:
    LedgerEntry created;
case LEDGER_ENTRY_UPDATED:
    LedgerEntry updated;
case LEDGER_ENTRY_REMOVED:
    LedgerKey removed;
case LEDGER_ENTRY_STATE:
    LedgerEntry state;
};

typedef LedgerEntryChange LedgerEntryChanges<>;

struct OperationMeta
{
    LedgerEntryChanges changes;
};

struct TransactionMetaV1
{
    LedgerEntryChanges txChanges; // tx level changes if any
    OperationMeta operations<>;   // meta for each operation
};

union TransactionMeta switch (int v)
{
case 0:
    OperationMeta operations<>;
case 1:
    TransactionMetaV1 v1;
};
}
