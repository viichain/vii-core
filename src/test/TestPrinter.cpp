
#include "test/TestPrinter.h"
#include "lib/util/format.h"
#include "test/TestMarket.h"

namespace Catch
{
std::string
StringMaker<viichain::OfferState>::convert(viichain::OfferState const& os)
{
    return fmt::format(
        "selling: {}, buying: {}, price: {}, amount: {}, type: {}",
        xdr::xdr_to_string(os.selling), xdr::xdr_to_string(os.buying),
        xdr::xdr_to_string(os.price), os.amount,
        os.type == viichain::OfferType::PASSIVE ? "passive" : "active");
}

std::string
StringMaker<viichain::CatchupRange>::convert(viichain::CatchupRange const& cr)
{
    return fmt::format("[{}..{}], applyBuckets: {}", cr.mLedgers.mFirst,
                       cr.getLast(), cr.getBucketApplyLedger());
}

std::string
StringMaker<viichain::historytestutils::CatchupPerformedWork>::convert(
    viichain::historytestutils::CatchupPerformedWork const& cm)
{
    return fmt::format("{}, {}, {}, {}, {}, {}, {}, {}",
                       cm.mHistoryArchiveStatesDownloaded,
                       cm.mLedgersDownloaded, cm.mLedgersVerified,
                       cm.mLedgerChainsVerificationFailed,
                       cm.mBucketsDownloaded, cm.mBucketsApplied,
                       cm.mTransactionsDownloaded, cm.mTransactionsApplied);
}
}
