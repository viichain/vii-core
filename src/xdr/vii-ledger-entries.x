
%#include "xdr/vii-types.h"

namespace viichain
{

typedef PublicKey AccountID;
typedef opaque Thresholds[4];
typedef string string32<32>;
typedef string string64<64>;
typedef int64 SequenceNumber;
typedef uint64 TimePoint;
typedef opaque DataValue<64>;

typedef opaque AssetCode4[4];

typedef opaque AssetCode12[12];

enum AssetType
{
    ASSET_TYPE_NATIVE = 0,
    ASSET_TYPE_CREDIT_ALPHANUM4 = 1,
    ASSET_TYPE_CREDIT_ALPHANUM12 = 2
};

union Asset switch (AssetType type)
{
case ASSET_TYPE_NATIVE: // Not credit
    void;

case ASSET_TYPE_CREDIT_ALPHANUM4:
    struct
    {
        AssetCode4 assetCode;
        AccountID issuer;
    } alphaNum4;

case ASSET_TYPE_CREDIT_ALPHANUM12:
    struct
    {
        AssetCode12 assetCode;
        AccountID issuer;
    } alphaNum12;

    };

struct Price
{
    int32 n; // numerator
    int32 d; // denominator
};

struct Liabilities
{
    int64 buying;
    int64 selling;
};

enum ThresholdIndexes
{
    THRESHOLD_MASTER_WEIGHT = 0,
    THRESHOLD_LOW = 1,
    THRESHOLD_MED = 2,
    THRESHOLD_HIGH = 3
};

enum LedgerEntryType
{
    ACCOUNT = 0,
    TRUSTLINE = 1,
    OFFER = 2,
    DATA = 3
};

struct Signer
{
    SignerKey key;
    uint32 weight; // really only need 1 byte
};

enum AccountFlags
{ // masks for each flag

                AUTH_REQUIRED_FLAG = 0x1,
            AUTH_REVOCABLE_FLAG = 0x2,
        AUTH_IMMUTABLE_FLAG = 0x4
};

const MASK_ACCOUNT_FLAGS = 0x7;

struct AccountEntry
{
    AccountID accountID;      // master public key for this account
    int64 balance;            // in stroops
    SequenceNumber seqNum;    // last sequence number used for this account
    uint32 numSubEntries;     // number of sub-entries this account has
                                  AccountID* inflationDest; // Account to vote for during inflation
    uint32 flags;             // see AccountFlags

    string32 homeDomain; // can be used for reverse federation and memo lookup

            Thresholds thresholds;

    Signer signers<20>; // possible signers for this account

        union switch (int v)
    {
    case 0:
        void;
    case 1:
        struct
        {
            Liabilities liabilities;

            union switch (int v)
            {
            case 0:
                void;
            }
            ext;
        } v1;
    }
    ext;
};

enum TrustLineFlags
{
        AUTHORIZED_FLAG = 1
};

const MASK_TRUSTLINE_FLAGS = 1;

struct TrustLineEntry
{
    AccountID accountID; // account this trustline belongs to
    Asset asset;         // type of asset (with issuer)
    int64 balance;       // how much of this asset the user has.
                         
    int64 limit;  // balance cannot be above this
    uint32 flags; // see TrustLineFlags

        union switch (int v)
    {
    case 0:
        void;
    case 1:
        struct
        {
            Liabilities liabilities;

            union switch (int v)
            {
            case 0:
                void;
            }
            ext;
        } v1;
    }
    ext;
};

enum OfferEntryFlags
{
        PASSIVE_FLAG = 1
};

const MASK_OFFERENTRY_FLAGS = 1;

struct OfferEntry
{
    AccountID sellerID;
    int64 offerID;
    Asset selling; // A
    Asset buying;  // B
    int64 amount;  // amount of A

        Price price;
    uint32 flags; // see OfferEntryFlags

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

struct DataEntry
{
    AccountID accountID; // account this data belongs to
    string64 dataName;
    DataValue dataValue;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

struct LedgerEntry
{
    uint32 lastModifiedLedgerSeq; // ledger the LedgerEntry was last changed

    union switch (LedgerEntryType type)
    {
    case ACCOUNT:
        AccountEntry account;
    case TRUSTLINE:
        TrustLineEntry trustLine;
    case OFFER:
        OfferEntry offer;
    case DATA:
        DataEntry data;
    }
    data;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

enum EnvelopeType
{
    ENVELOPE_TYPE_SCP = 1,
    ENVELOPE_TYPE_TX = 2,
    ENVELOPE_TYPE_AUTH = 3,
    ENVELOPE_TYPE_SCPVALUE = 4
};
}
