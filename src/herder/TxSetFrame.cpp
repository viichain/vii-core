
#include "util/asio.h"
#include "TxSetFrame.h"
#include "crypto/Hex.h"
#include "crypto/Random.h"
#include "crypto/SHA.h"
#include "database/Database.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "main/Application.h"
#include "main/Config.h"
#include "transactions/TransactionUtils.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include "xdrpp/marshal.h"
#include <algorithm>
#include <list>
#include <numeric>

#include "xdrpp/printer.h"

namespace viichain
{

using namespace std;

TxSetFrame::TxSetFrame(Hash const& previousLedgerHash)
    : mHashIsValid(false), mPreviousLedgerHash(previousLedgerHash)
{
}

TxSetFrame::TxSetFrame(Hash const& networkID, TransactionSet const& xdrSet)
    : mHashIsValid(false)
{
    for (auto const& txEnvelope : xdrSet.txs)
    {
        TransactionFramePtr tx =
            TransactionFrame::makeTransactionFromWire(networkID, txEnvelope);
        mTransactions.push_back(tx);
    }
    mPreviousLedgerHash = xdrSet.previousLedgerHash;
}

static bool
HashTxSorter(TransactionFramePtr const& tx1, TransactionFramePtr const& tx2)
{
            return tx1->getFullHash() < tx2->getFullHash();
}

void
TxSetFrame::sortForHash()
{
    std::sort(mTransactions.begin(), mTransactions.end(), HashTxSorter);
    mHashIsValid = false;
}

struct ApplyTxSorter
{
    Hash mSetHash;
    ApplyTxSorter(Hash h) : mSetHash{std::move(h)}
    {
    }

    bool
    operator()(TransactionFramePtr const& tx1,
               TransactionFramePtr const& tx2) const
    {
                        return lessThanXored(tx1->getFullHash(), tx2->getFullHash(), mSetHash);
    }
};

static bool
SeqSorter(TransactionFramePtr const& tx1, TransactionFramePtr const& tx2)
{
    return tx1->getSeqNum() < tx2->getSeqNum();
}

std::vector<TransactionFramePtr>
TxSetFrame::sortForApply()
{
    auto txQueues = buildAccountTxQueues();

                std::list<std::deque<TransactionFramePtr>> txBatches;

    while (!txQueues.empty())
    {
        txBatches.emplace_back();
        auto& curBatch = txBatches.back();
                for (auto it = txQueues.begin(); it != txQueues.end();)
        {
            auto& h = it->second.front();
            curBatch.emplace_back(h);
            it->second.pop_front();
            if (it->second.empty())
            {
                                it = txQueues.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    vector<TransactionFramePtr> retList;
    retList.reserve(mTransactions.size());
    for (auto& batch : txBatches)
    {
                        ApplyTxSorter s(getContentsHash());
        std::sort(batch.begin(), batch.end(), s);
        for (auto const& tx : batch)
        {
            retList.push_back(tx);
        }
    }

    return retList;
}

struct SurgeCompare
{
    Hash mSeed;
    LedgerHeader const& mHeader;
    SurgeCompare(LedgerHeader const& header)
        : mSeed(HashUtils::random()), mHeader(header)
    {
    }

        bool
    operator()(TxSetFrame::AccountTransactionQueue const* tx1,
               TxSetFrame::AccountTransactionQueue const* tx2) const
    {
        if (tx1 == nullptr || tx1->empty())
        {
            return tx2 ? !tx2->empty() : false;
        }
        if (tx2 == nullptr || tx2->empty())
        {
            return false;
        }

        auto& top1 = tx1->front();
        auto& top2 = tx2->front();

                auto v1 = bigMultiply(top1->getFeeBid(), top2->getMinFee(mHeader));
        auto v2 = bigMultiply(top2->getFeeBid(), top1->getMinFee(mHeader));
        if (v1 < v2)
        {
            return true;
        }
        else if (v1 > v2)
        {
            return false;
        }
                return lessThanXored(top1->getFullHash(), top2->getFullHash(), mSeed);
    }
};

std::unordered_map<AccountID, TxSetFrame::AccountTransactionQueue>
TxSetFrame::buildAccountTxQueues()
{
    std::unordered_map<AccountID, AccountTransactionQueue> actTxQueueMap;
    for (auto& tx : mTransactions)
    {
        auto& id = tx->getSourceID();
        auto it = actTxQueueMap.find(id);
        if (it == actTxQueueMap.end())
        {
            auto d = std::make_pair(id, AccountTransactionQueue{});
            auto r = actTxQueueMap.insert(d);
            it = r.first;
        }
        it->second.emplace_back(tx);
    }

    for (auto& am : actTxQueueMap)
    {
                std::sort(am.second.begin(), am.second.end(), SeqSorter);
    }
    return actTxQueueMap;
}

void
TxSetFrame::surgePricingFilter(Application& app)
{
    LedgerTxn ltx(app.getLedgerTxnRoot());
    auto header = ltx.loadHeader();

    bool maxIsOps = header.current().ledgerVersion >= 11;

    size_t opsLeft;
    {
        size_t maxTxSetSize = header.current().maxTxSetSize;
        opsLeft = maxIsOps ? maxTxSetSize : (maxTxSetSize * MAX_OPS_PER_TX);
    }

    auto curSizeOps = maxIsOps ? sizeOp() : (sizeTx() * MAX_OPS_PER_TX);
    if (curSizeOps > opsLeft)
    {
        CLOG(WARNING, "Herder")
            << "surge pricing in effect! " << curSizeOps << " > " << opsLeft;

        auto actTxQueueMap = buildAccountTxQueues();

        auto headerCopy = header.current();
        SurgeCompare const surge(headerCopy);
        std::priority_queue<AccountTransactionQueue*,
                            std::vector<AccountTransactionQueue*>, SurgeCompare>
            surgeQueue(surge);

        for (auto& am : actTxQueueMap)
        {
            surgeQueue.push(&am.second);
        }

        std::vector<TransactionFramePtr> updatedSet;
        updatedSet.reserve(mTransactions.size());
        while (opsLeft > 0 && !surgeQueue.empty())
        {
            auto cur = surgeQueue.top();
            surgeQueue.pop();
                        auto& curTopTx = cur->front();
            size_t opsCount =
                maxIsOps ? curTopTx->getOperations().size() : MAX_OPS_PER_TX;
            if (opsCount <= opsLeft)
            {
                                updatedSet.emplace_back(curTopTx);
                cur->pop_front();
                opsLeft -= opsCount;
                                if (!cur->empty())
                {
                    surgeQueue.push(cur);
                }
            }
            else
            {
                                cur->clear();
            }
        }
        mTransactions = std::move(updatedSet);
        sortForHash();
    }
}

bool
TxSetFrame::checkOrTrim(
    Application& app,
    std::function<bool(TransactionFramePtr, SequenceNumber)>
        processInvalidTxLambda,
    std::function<bool(std::deque<TransactionFramePtr> const&)>
        processInsufficientBalance)
{
    LedgerTxn ltx(app.getLedgerTxnRoot());

    auto accountTxMap = buildAccountTxQueues();

    Hash lastHash;
    for (auto& tx : mTransactions)
    {
        if (tx->getFullHash() < lastHash)
        {
            CLOG(DEBUG, "Herder")
                << "bad txSet: " << hexAbbrev(mPreviousLedgerHash)
                << " not sorted correctly";
            return false;
        }
        lastHash = tx->getFullHash();
    }

    for (auto& item : accountTxMap)
    {
        TransactionFramePtr lastTx;
        SequenceNumber lastSeq = 0;
        int64_t totFee = 0;
        for (auto& tx : item.second)
        {
            if (!tx->checkValid(ltx, lastSeq))
            {
                if (processInvalidTxLambda(tx, lastSeq))
                    continue;

                return false;
            }
            totFee += tx->getFeeBid();

            lastTx = tx;
            lastSeq = tx->getSeqNum();
        }
        if (lastTx)
        {
                        auto const& source =
                viichain::loadAccount(ltx, lastTx->getSourceID());
            if (getAvailableBalance(ltx.loadHeader(), source) < totFee)
            {
                if (!processInsufficientBalance(item.second))
                    return false;
            }
        }
    }

    return true;
}

std::vector<TransactionFramePtr>
TxSetFrame::trimInvalid(Application& app)
{
    std::vector<TransactionFramePtr> trimmed;
    sortForHash();

    auto processInvalidTxLambda = [&](TransactionFramePtr tx,
                                      SequenceNumber lastSeq) {
        trimmed.push_back(tx);
        removeTx(tx);
        return true;
    };
    auto processInsufficientBalance =
        [&](deque<TransactionFramePtr> const& item) {
            for (auto& tx : item)
            {
                trimmed.push_back(tx);
                removeTx(tx);
            }
            return true;
        };

    checkOrTrim(app, processInvalidTxLambda, processInsufficientBalance);
    return trimmed;
}

bool
TxSetFrame::checkValid(Application& app)
{
    auto& lcl = app.getLedgerManager().getLastClosedLedgerHeader();
        if (lcl.hash != mPreviousLedgerHash)
    {
        CLOG(DEBUG, "Herder")
            << "Got bad txSet: " << hexAbbrev(mPreviousLedgerHash)
            << " ; expected: " << hexAbbrev(lcl.hash);
        return false;
    }

    if (this->size(lcl.header) > lcl.header.maxTxSetSize)
    {
        CLOG(DEBUG, "Herder")
            << "Got bad txSet: too many txs " << this->size(lcl.header) << " > "
            << lcl.header.maxTxSetSize;
        return false;
    }

    auto processInvalidTxLambda = [&](TransactionFramePtr tx,
                                      SequenceNumber const& lastSeq) {
        CLOG(DEBUG, "Herder")
            << "bad txSet: " << hexAbbrev(mPreviousLedgerHash) << " tx invalid"
            << " lastSeq:" << lastSeq
            << " tx: " << xdr::xdr_to_string(tx->getEnvelope())
            << " result: " << tx->getResultCode();

        return false;
    };
    auto processInsufficientBalance =
        [&](deque<TransactionFramePtr> const& item) {
            CLOG(DEBUG, "Herder")
                << "bad txSet: " << hexAbbrev(mPreviousLedgerHash)
                << " account can't pay fee"
                << " tx:" << xdr::xdr_to_string(item.back()->getEnvelope());

            return false;
        };
    return checkOrTrim(app, processInvalidTxLambda, processInsufficientBalance);
}

void
TxSetFrame::removeTx(TransactionFramePtr tx)
{
    auto it = std::find(mTransactions.begin(), mTransactions.end(), tx);
    if (it != mTransactions.end())
        mTransactions.erase(it);
    mHashIsValid = false;
}

Hash
TxSetFrame::getContentsHash()
{
    if (!mHashIsValid)
    {
        sortForHash();
        auto hasher = SHA256::create();
        hasher->add(mPreviousLedgerHash);
        for (unsigned int n = 0; n < mTransactions.size(); n++)
        {
            hasher->add(xdr::xdr_to_opaque(mTransactions[n]->getEnvelope()));
        }
        mHash = hasher->finish();
        mHashIsValid = true;
    }
    return mHash;
}

Hash&
TxSetFrame::previousLedgerHash()
{
    mHashIsValid = false;
    return mPreviousLedgerHash;
}

Hash const&
TxSetFrame::previousLedgerHash() const
{
    return mPreviousLedgerHash;
}

size_t
TxSetFrame::size(LedgerHeader const& lh) const
{
    return lh.ledgerVersion >= 11 ? sizeOp() : sizeTx();
}

size_t
TxSetFrame::sizeOp() const
{
    return std::accumulate(
        mTransactions.begin(), mTransactions.end(), size_t(0),
        [](size_t a, TransactionFramePtr const& tx) {
            return a + tx->getEnvelope().tx.operations.size();
        });
}

int64_t
TxSetFrame::getBaseFee(LedgerHeader const& lh) const
{
    int64_t baseFee = lh.baseFee;
    if (lh.ledgerVersion >= 11)
    {
        size_t ops = 0;
        int64_t lowBaseFee = std::numeric_limits<int64_t>::max();
        for (auto& txPtr : mTransactions)
        {
            auto txOps = txPtr->getEnvelope().tx.operations.size();
            ops += txOps;
            int64_t txBaseFee =
                bigDivide(txPtr->getFeeBid(), 1, static_cast<int64_t>(txOps),
                          Rounding::ROUND_UP);
            lowBaseFee = std::min(lowBaseFee, txBaseFee);
        }
                        size_t surgeOpsCutoff = 0;
        if (lh.maxTxSetSize >= MAX_OPS_PER_TX)
        {
            surgeOpsCutoff = lh.maxTxSetSize - MAX_OPS_PER_TX;
        }
        if (ops > surgeOpsCutoff)
        {
            baseFee = lowBaseFee;
        }
    }
    return baseFee;
}

int64_t
TxSetFrame::getTotalFees(LedgerHeader const& lh) const
{
    auto baseFee = getBaseFee(lh);
    return std::accumulate(mTransactions.begin(), mTransactions.end(),
                           int64_t(0),
                           [&](int64_t t, TransactionFramePtr const& tx) {
                               return t + tx->getFee(lh, baseFee);
                           });
}

void
TxSetFrame::toXDR(TransactionSet& txSet)
{
    txSet.txs.resize(xdr::size32(mTransactions.size()));
    for (unsigned int n = 0; n < mTransactions.size(); n++)
    {
        txSet.txs[n] = mTransactions[n]->getEnvelope();
    }
    txSet.previousLedgerHash = mPreviousLedgerHash;
}
} // namespace viichain
