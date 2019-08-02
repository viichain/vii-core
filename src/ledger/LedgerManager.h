#pragma once


#include "catchup/CatchupManager.h"
#include "history/HistoryManager.h"
#include <memory>

namespace viichain
{

class LedgerCloseData;
class Database;

class LedgerManager
{
  public:
    static const uint32_t GENESIS_LEDGER_SEQ;
    static const uint32_t GENESIS_LEDGER_VERSION;
    static const uint32_t GENESIS_LEDGER_BASE_FEE;
    static const uint32_t GENESIS_LEDGER_BASE_RESERVE;
    static const uint32_t GENESIS_LEDGER_MAX_TX_SIZE;
    static const int64_t GENESIS_LEDGER_TOTAL_COINS;

    enum State
    {
                LM_BOOTING_STATE,

                        LM_SYNCED_STATE,

                        LM_CATCHING_UP_STATE,

        LM_NUM_STATE
    };

    enum class CatchupState
    {
        NONE,
        WAITING_FOR_TRIGGER_LEDGER,
        APPLYING_HISTORY,
        APPLYING_BUFFERED_LEDGERS,
        WAITING_FOR_CLOSING_LEDGER
    };

    virtual void bootstrap() = 0;
    virtual State getState() const = 0;
    virtual CatchupState getCatchupState() const = 0;
    virtual std::string getStateHuman() const = 0;

    bool
    isSynced() const
    {
        return getState() == LM_SYNCED_STATE;
    }

        static std::string ledgerAbbrev(uint32_t seq, uint256 const& hash);
    static std::string ledgerAbbrev(LedgerHeader const& header);
    static std::string ledgerAbbrev(LedgerHeader const& header,
                                    uint256 const& hash);
    static std::string ledgerAbbrev(LedgerHeaderHistoryEntry he);

        static std::unique_ptr<LedgerManager> create(Application& app);

        static LedgerHeader genesisLedger();

                    virtual void valueExternalized(LedgerCloseData const& ledgerData) = 0;

        virtual LedgerHeaderHistoryEntry const&
    getLastClosedLedgerHeader() const = 0;

            virtual HistoryArchiveState getLastClosedLedgerHAS() = 0;

        virtual uint32_t getLastClosedLedgerNum() const = 0;

                virtual int64_t getLastMinBalance(uint32_t ownerCount) const = 0;

    virtual uint32_t getLastReserve() const = 0;

        virtual uint32_t getLastTxFee() const = 0;

            virtual uint32_t getLastMaxTxSetSize() const = 0;

        virtual uint64_t secondsSinceLastLedgerClose() const = 0;

            virtual void syncMetrics() = 0;

    virtual Database& getDatabase() = 0;

        virtual void startNewLedger() = 0;

            virtual void loadLastKnownLedger(
        std::function<void(asio::error_code const& ec)> handler) = 0;

                            virtual void startCatchup(CatchupConfiguration configuration) = 0;

                    virtual void closeLedger(LedgerCloseData const& ledgerData) = 0;

        virtual void deleteOldEntries(Database& db, uint32_t ledgerSeq,
                                  uint32_t count) = 0;

    virtual ~LedgerManager()
    {
    }
};
}
