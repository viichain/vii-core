
%#include "xdr/vii-ledger-entries.h"

namespace viichain
{

struct DecoratedSignature
{
    SignatureHint hint;  // last 4 bytes of the public key, used as a hint
    Signature signature; // actual signature
};

enum OperationType
{
    CREATE_ACCOUNT = 0,
    PAYMENT = 1,
    PATH_PAYMENT = 2,
    MANAGE_SELL_OFFER = 3,
    CREATE_PASSIVE_SELL_OFFER = 4,
    SET_OPTIONS = 5,
    CHANGE_TRUST = 6,
    ALLOW_TRUST = 7,
    ACCOUNT_MERGE = 8,
    INFLATION = 9,
    MANAGE_DATA = 10,
    BUMP_SEQUENCE = 11,
    MANAGE_BUY_OFFER = 12
};

struct CreateAccountOp
{
    AccountID destination; // account to create
    int64 startingBalance; // amount they end up with
};

struct PaymentOp
{
    AccountID destination; // recipient of the payment
    Asset asset;           // what they end up with
    int64 amount;          // amount they end up with
};

struct PathPaymentOp
{
    Asset sendAsset; // asset we pay with
    int64 sendMax;   // the maximum amount of sendAsset to
                                          
    AccountID destination; // recipient of the payment
    Asset destAsset;       // what they end up with
    int64 destAmount;      // amount they end up with

    Asset path<5>; // additional hops it must go through to get there
};

struct ManageSellOfferOp
{
    Asset selling;
    Asset buying;
    int64 amount; // amount being sold. if set to 0, delete the offer
    Price price;  // price of thing being sold in terms of what you are buying

        int64 offerID;
};

struct ManageBuyOfferOp
{
    Asset selling;
    Asset buying;
    int64 buyAmount; // amount being bought. if set to 0, delete the offer
    Price price;     // price of thing being bought in terms of what you are
                     
        int64 offerID;
};

struct CreatePassiveSellOfferOp
{
    Asset selling; // A
    Asset buying;  // B
    int64 amount;  // amount taker gets. if set to 0, delete the offer
    Price price;   // cost of A in terms of B
};

struct SetOptionsOp
{
    AccountID* inflationDest; // sets the inflation destination

    uint32* clearFlags; // which flags to clear
    uint32* setFlags;   // which flags to set

        uint32* masterWeight; // weight of the master account
    uint32* lowThreshold;
    uint32* medThreshold;
    uint32* highThreshold;

    string32* homeDomain; // sets the home domain

            Signer* signer;
};

struct ChangeTrustOp
{
    Asset line;

        int64 limit;
};

struct AllowTrustOp
{
    AccountID trustor;
    union switch (AssetType type)
    {
        case ASSET_TYPE_CREDIT_ALPHANUM4:
        AssetCode4 assetCode4;

    case ASSET_TYPE_CREDIT_ALPHANUM12:
        AssetCode12 assetCode12;

            }
    asset;

    bool authorize;
};

struct ManageDataOp
{
    string64 dataName;
    DataValue* dataValue; // set to null to clear
};

struct BumpSequenceOp
{
    SequenceNumber bumpTo;
};

struct Operation
{
                AccountID* sourceAccount;

    union switch (OperationType type)
    {
    case CREATE_ACCOUNT:
        CreateAccountOp createAccountOp;
    case PAYMENT:
        PaymentOp paymentOp;
    case PATH_PAYMENT:
        PathPaymentOp pathPaymentOp;
    case MANAGE_SELL_OFFER:
        ManageSellOfferOp manageSellOfferOp;
    case CREATE_PASSIVE_SELL_OFFER:
        CreatePassiveSellOfferOp createPassiveSellOfferOp;
    case SET_OPTIONS:
        SetOptionsOp setOptionsOp;
    case CHANGE_TRUST:
        ChangeTrustOp changeTrustOp;
    case ALLOW_TRUST:
        AllowTrustOp allowTrustOp;
    case ACCOUNT_MERGE:
        AccountID destination;
    case INFLATION:
        void;
    case MANAGE_DATA:
        ManageDataOp manageDataOp;
    case BUMP_SEQUENCE:
        BumpSequenceOp bumpSequenceOp;
    case MANAGE_BUY_OFFER:
        ManageBuyOfferOp manageBuyOfferOp;
    }
    body;
};

enum MemoType
{
    MEMO_NONE = 0,
    MEMO_TEXT = 1,
    MEMO_ID = 2,
    MEMO_HASH = 3,
    MEMO_RETURN = 4
};

union Memo switch (MemoType type)
{
case MEMO_NONE:
    void;
case MEMO_TEXT:
    string text<128>;
case MEMO_ID:
    uint64 id;
case MEMO_HASH:
    Hash hash; // the hash of what to pull from the content server
case MEMO_RETURN:
    Hash retHash; // the hash of the tx you are rejecting
};

struct TimeBounds
{
    TimePoint minTime;
    TimePoint maxTime; // 0 here means no maxTime
};

const MAX_OPS_PER_TX = 100;

struct Transaction
{
        AccountID sourceAccount;

        uint32 fee;

        SequenceNumber seqNum;

        TimeBounds* timeBounds;

    Memo memo;

    Operation operations<MAX_OPS_PER_TX>;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};

struct TransactionSignaturePayload
{
    Hash networkId;
    union switch (EnvelopeType type)
    {
    case ENVELOPE_TYPE_TX:
        Transaction tx;
            }
    taggedTransaction;
};

struct TransactionEnvelope
{
    Transaction tx;
        DecoratedSignature signatures<20>;
};

struct ClaimOfferAtom
{
        AccountID sellerID; // Account that owns the offer
    int64 offerID;

        Asset assetSold;
    int64 amountSold;

        Asset assetBought;
    int64 amountBought;
};

enum CreateAccountResultCode
{
        CREATE_ACCOUNT_SUCCESS = 0, // account was created

        CREATE_ACCOUNT_MALFORMED = -1,   // invalid destination
    CREATE_ACCOUNT_UNDERFUNDED = -2, // not enough funds in source account
    CREATE_ACCOUNT_LOW_RESERVE =
        -3, // would create an account below the min reserve
    CREATE_ACCOUNT_ALREADY_EXIST = -4 // account already exists
};

union CreateAccountResult switch (CreateAccountResultCode code)
{
case CREATE_ACCOUNT_SUCCESS:
    void;
default:
    void;
};

enum PaymentResultCode
{
        PAYMENT_SUCCESS = 0, // payment successfuly completed

        PAYMENT_MALFORMED = -1,          // bad input
    PAYMENT_UNDERFUNDED = -2,        // not enough funds in source account
    PAYMENT_SRC_NO_TRUST = -3,       // no trust line on source account
    PAYMENT_SRC_NOT_AUTHORIZED = -4, // source not authorized to transfer
    PAYMENT_NO_DESTINATION = -5,     // destination account does not exist
    PAYMENT_NO_TRUST = -6,       // destination missing a trust line for asset
    PAYMENT_NOT_AUTHORIZED = -7, // destination not authorized to hold asset
    PAYMENT_LINE_FULL = -8,      // destination would go above their limit
    PAYMENT_NO_ISSUER = -9       // missing issuer on asset
};

union PaymentResult switch (PaymentResultCode code)
{
case PAYMENT_SUCCESS:
    void;
default:
    void;
};

enum PathPaymentResultCode
{
        PATH_PAYMENT_SUCCESS = 0, // success

        PATH_PAYMENT_MALFORMED = -1,          // bad input
    PATH_PAYMENT_UNDERFUNDED = -2,        // not enough funds in source account
    PATH_PAYMENT_SRC_NO_TRUST = -3,       // no trust line on source account
    PATH_PAYMENT_SRC_NOT_AUTHORIZED = -4, // source not authorized to transfer
    PATH_PAYMENT_NO_DESTINATION = -5,     // destination account does not exist
    PATH_PAYMENT_NO_TRUST = -6,           // dest missing a trust line for asset
    PATH_PAYMENT_NOT_AUTHORIZED = -7,     // dest not authorized to hold asset
    PATH_PAYMENT_LINE_FULL = -8,          // dest would go above their limit
    PATH_PAYMENT_NO_ISSUER = -9,          // missing issuer on one asset
    PATH_PAYMENT_TOO_FEW_OFFERS = -10,    // not enough offers to satisfy path
    PATH_PAYMENT_OFFER_CROSS_SELF = -11,  // would cross one of its own offers
    PATH_PAYMENT_OVER_SENDMAX = -12       // could not satisfy sendmax
};

struct SimplePaymentResult
{
    AccountID destination;
    Asset asset;
    int64 amount;
};

union PathPaymentResult switch (PathPaymentResultCode code)
{
case PATH_PAYMENT_SUCCESS:
    struct
    {
        ClaimOfferAtom offers<>;
        SimplePaymentResult last;
    } success;
case PATH_PAYMENT_NO_ISSUER:
    Asset noIssuer; // the asset that caused the error
default:
    void;
};

enum ManageSellOfferResultCode
{
        MANAGE_SELL_OFFER_SUCCESS = 0,

        MANAGE_SELL_OFFER_MALFORMED = -1,     // generated offer would be invalid
    MANAGE_SELL_OFFER_SELL_NO_TRUST = -2, // no trust line for what we're selling
    MANAGE_SELL_OFFER_BUY_NO_TRUST = -3,  // no trust line for what we're buying
    MANAGE_SELL_OFFER_SELL_NOT_AUTHORIZED = -4, // not authorized to sell
    MANAGE_SELL_OFFER_BUY_NOT_AUTHORIZED = -5,  // not authorized to buy
    MANAGE_SELL_OFFER_LINE_FULL = -6,      // can't receive more of what it's buying
    MANAGE_SELL_OFFER_UNDERFUNDED = -7,    // doesn't hold what it's trying to sell
    MANAGE_SELL_OFFER_CROSS_SELF = -8,     // would cross an offer from the same user
    MANAGE_SELL_OFFER_SELL_NO_ISSUER = -9, // no issuer for what we're selling
    MANAGE_SELL_OFFER_BUY_NO_ISSUER = -10, // no issuer for what we're buying

        MANAGE_SELL_OFFER_NOT_FOUND = -11, // offerID does not match an existing offer

    MANAGE_SELL_OFFER_LOW_RESERVE = -12 // not enough funds to create a new Offer
};

enum ManageOfferEffect
{
    MANAGE_OFFER_CREATED = 0,
    MANAGE_OFFER_UPDATED = 1,
    MANAGE_OFFER_DELETED = 2
};

struct ManageOfferSuccessResult
{
        ClaimOfferAtom offersClaimed<>;

    union switch (ManageOfferEffect effect)
    {
    case MANAGE_OFFER_CREATED:
    case MANAGE_OFFER_UPDATED:
        OfferEntry offer;
    default:
        void;
    }
    offer;
};

union ManageSellOfferResult switch (ManageSellOfferResultCode code)
{
case MANAGE_SELL_OFFER_SUCCESS:
    ManageOfferSuccessResult success;
default:
    void;
};

enum ManageBuyOfferResultCode
{
        MANAGE_BUY_OFFER_SUCCESS = 0,

        MANAGE_BUY_OFFER_MALFORMED = -1,     // generated offer would be invalid
    MANAGE_BUY_OFFER_SELL_NO_TRUST = -2, // no trust line for what we're selling
    MANAGE_BUY_OFFER_BUY_NO_TRUST = -3,  // no trust line for what we're buying
    MANAGE_BUY_OFFER_SELL_NOT_AUTHORIZED = -4, // not authorized to sell
    MANAGE_BUY_OFFER_BUY_NOT_AUTHORIZED = -5,  // not authorized to buy
    MANAGE_BUY_OFFER_LINE_FULL = -6,      // can't receive more of what it's buying
    MANAGE_BUY_OFFER_UNDERFUNDED = -7,    // doesn't hold what it's trying to sell
    MANAGE_BUY_OFFER_CROSS_SELF = -8,     // would cross an offer from the same user
    MANAGE_BUY_OFFER_SELL_NO_ISSUER = -9, // no issuer for what we're selling
    MANAGE_BUY_OFFER_BUY_NO_ISSUER = -10, // no issuer for what we're buying

        MANAGE_BUY_OFFER_NOT_FOUND = -11, // offerID does not match an existing offer

    MANAGE_BUY_OFFER_LOW_RESERVE = -12 // not enough funds to create a new Offer
};

union ManageBuyOfferResult switch (ManageBuyOfferResultCode code)
{
case MANAGE_BUY_OFFER_SUCCESS:
    ManageOfferSuccessResult success;
default:
    void;
};

enum SetOptionsResultCode
{
        SET_OPTIONS_SUCCESS = 0,
        SET_OPTIONS_LOW_RESERVE = -1,      // not enough funds to add a signer
    SET_OPTIONS_TOO_MANY_SIGNERS = -2, // max number of signers already reached
    SET_OPTIONS_BAD_FLAGS = -3,        // invalid combination of clear/set flags
    SET_OPTIONS_INVALID_INFLATION = -4,      // inflation account does not exist
    SET_OPTIONS_CANT_CHANGE = -5,            // can no longer change this option
    SET_OPTIONS_UNKNOWN_FLAG = -6,           // can't set an unknown flag
    SET_OPTIONS_THRESHOLD_OUT_OF_RANGE = -7, // bad value for weight/threshold
    SET_OPTIONS_BAD_SIGNER = -8,             // signer cannot be masterkey
    SET_OPTIONS_INVALID_HOME_DOMAIN = -9     // malformed home domain
};

union SetOptionsResult switch (SetOptionsResultCode code)
{
case SET_OPTIONS_SUCCESS:
    void;
default:
    void;
};

enum ChangeTrustResultCode
{
        CHANGE_TRUST_SUCCESS = 0,
        CHANGE_TRUST_MALFORMED = -1,     // bad input
    CHANGE_TRUST_NO_ISSUER = -2,     // could not find issuer
    CHANGE_TRUST_INVALID_LIMIT = -3, // cannot drop limit below balance
                                         CHANGE_TRUST_LOW_RESERVE =
        -4, // not enough funds to create a new trust line,
    CHANGE_TRUST_SELF_NOT_ALLOWED = -5  // trusting self is not allowed
};

union ChangeTrustResult switch (ChangeTrustResultCode code)
{
case CHANGE_TRUST_SUCCESS:
    void;
default:
    void;
};

enum AllowTrustResultCode
{
        ALLOW_TRUST_SUCCESS = 0,
        ALLOW_TRUST_MALFORMED = -1,     // asset is not ASSET_TYPE_ALPHANUM
    ALLOW_TRUST_NO_TRUST_LINE = -2, // trustor does not have a trustline
                                        ALLOW_TRUST_TRUST_NOT_REQUIRED = -3,
    ALLOW_TRUST_CANT_REVOKE = -4,     // source account can't revoke trust,
    ALLOW_TRUST_SELF_NOT_ALLOWED = -5 // trusting self is not allowed
};

union AllowTrustResult switch (AllowTrustResultCode code)
{
case ALLOW_TRUST_SUCCESS:
    void;
default:
    void;
};

enum AccountMergeResultCode
{
        ACCOUNT_MERGE_SUCCESS = 0,
        ACCOUNT_MERGE_MALFORMED = -1,       // can't merge onto itself
    ACCOUNT_MERGE_NO_ACCOUNT = -2,      // destination does not exist
    ACCOUNT_MERGE_IMMUTABLE_SET = -3,   // source account has AUTH_IMMUTABLE set
    ACCOUNT_MERGE_HAS_SUB_ENTRIES = -4, // account has trust lines/offers
    ACCOUNT_MERGE_SEQNUM_TOO_FAR = -5,  // sequence number is over max allowed
    ACCOUNT_MERGE_DEST_FULL = -6        // can't add source balance to
                                        };

union AccountMergeResult switch (AccountMergeResultCode code)
{
case ACCOUNT_MERGE_SUCCESS:
    int64 sourceAccountBalance; // how much got transfered from source account
default:
    void;
};

enum InflationResultCode
{
        INFLATION_SUCCESS = 0,
        INFLATION_NOT_TIME = -1
};

struct InflationPayout // or use PaymentResultAtom to limit types?
{
    AccountID destination;
    int64 amount;
};

union InflationResult switch (InflationResultCode code)
{
case INFLATION_SUCCESS:
    InflationPayout payouts<>;
default:
    void;
};

enum ManageDataResultCode
{
        MANAGE_DATA_SUCCESS = 0,
        MANAGE_DATA_NOT_SUPPORTED_YET =
        -1, // The network hasn't moved to this protocol change yet
    MANAGE_DATA_NAME_NOT_FOUND =
        -2, // Trying to remove a Data Entry that isn't there
    MANAGE_DATA_LOW_RESERVE = -3, // not enough funds to create a new Data Entry
    MANAGE_DATA_INVALID_NAME = -4 // Name not a valid string
};

union ManageDataResult switch (ManageDataResultCode code)
{
case MANAGE_DATA_SUCCESS:
    void;
default:
    void;
};

enum BumpSequenceResultCode
{
        BUMP_SEQUENCE_SUCCESS = 0,
        BUMP_SEQUENCE_BAD_SEQ = -1 // `bumpTo` is not within bounds
};

union BumpSequenceResult switch (BumpSequenceResultCode code)
{
case BUMP_SEQUENCE_SUCCESS:
    void;
default:
    void;
};
enum OperationResultCode
{
    opINNER = 0, // inner object result is valid

    opBAD_AUTH = -1,     // too few valid signatures / wrong network
    opNO_ACCOUNT = -2,   // source account was not found
    opNOT_SUPPORTED = -3, // operation not supported at this time
    opTOO_MANY_SUBENTRIES = -4, // max number of subentries already reached
    opEXCEEDED_WORK_LIMIT = -5  // operation did too much work
};

union OperationResult switch (OperationResultCode code)
{
case opINNER:
    union switch (OperationType type)
    {
    case CREATE_ACCOUNT:
        CreateAccountResult createAccountResult;
    case PAYMENT:
        PaymentResult paymentResult;
    case PATH_PAYMENT:
        PathPaymentResult pathPaymentResult;
    case MANAGE_SELL_OFFER:
        ManageSellOfferResult manageSellOfferResult;
    case CREATE_PASSIVE_SELL_OFFER:
        ManageSellOfferResult createPassiveSellOfferResult;
    case SET_OPTIONS:
        SetOptionsResult setOptionsResult;
    case CHANGE_TRUST:
        ChangeTrustResult changeTrustResult;
    case ALLOW_TRUST:
        AllowTrustResult allowTrustResult;
    case ACCOUNT_MERGE:
        AccountMergeResult accountMergeResult;
    case INFLATION:
        InflationResult inflationResult;
    case MANAGE_DATA:
        ManageDataResult manageDataResult;
    case BUMP_SEQUENCE:
        BumpSequenceResult bumpSeqResult;
    case MANAGE_BUY_OFFER:
	ManageBuyOfferResult manageBuyOfferResult;
    }
    tr;
default:
    void;
};

enum TransactionResultCode
{
    txSUCCESS = 0, // all operations succeeded

    txFAILED = -1, // one of the operations failed (none were applied)

    txTOO_EARLY = -2,         // ledger closeTime before minTime
    txTOO_LATE = -3,          // ledger closeTime after maxTime
    txMISSING_OPERATION = -4, // no operation was specified
    txBAD_SEQ = -5,           // sequence number does not match source account

    txBAD_AUTH = -6,             // too few valid signatures / wrong network
    txINSUFFICIENT_BALANCE = -7, // fee would bring account below reserve
    txNO_ACCOUNT = -8,           // source account not found
    txINSUFFICIENT_FEE = -9,     // fee is too small
    txBAD_AUTH_EXTRA = -10,      // unused signatures attached to transaction
    txINTERNAL_ERROR = -11       // an unknown error occured
};

struct TransactionResult
{
    int64 feeCharged; // actual fee charged for the transaction

    union switch (TransactionResultCode code)
    {
    case txSUCCESS:
    case txFAILED:
        OperationResult results<>;
    default:
        void;
    }
    result;

        union switch (int v)
    {
    case 0:
        void;
    }
    ext;
};
}
