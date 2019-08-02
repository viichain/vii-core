#pragma once


#include "xdr/vii-ledger.h"

#include "main/Config.h"
#include "util/Timer.h"
#include "util/optional.h"
#include <stdint.h>
#include <vector>

namespace viichain
{
class AbstractLedgerTxn;
class Config;
class Database;
struct LedgerHeader;
struct LedgerUpgrade;

class Upgrades
{
  public:
    struct UpgradeParameters
    {
        UpgradeParameters()
        {
        }
        UpgradeParameters(Config const& cfg)
        {
            mUpgradeTime = cfg.TESTING_UPGRADE_DATETIME;
            mProtocolVersion =
                make_optional<uint32>(cfg.LEDGER_PROTOCOL_VERSION);
            mBaseFee = make_optional<uint32>(cfg.TESTING_UPGRADE_DESIRED_FEE);
            mMaxTxSize =
                make_optional<uint32>(cfg.TESTING_UPGRADE_MAX_TX_SET_SIZE);
            mBaseReserve = make_optional<uint32>(cfg.TESTING_UPGRADE_RESERVE);
        }
        VirtualClock::time_point mUpgradeTime;
        optional<uint32> mProtocolVersion;
        optional<uint32> mBaseFee;
        optional<uint32> mMaxTxSize;
        optional<uint32> mBaseReserve;

        std::string toJson() const;
        void fromJson(std::string const& s);
    };

    Upgrades()
    {
    }
    explicit Upgrades(UpgradeParameters const& params);

    void setParameters(UpgradeParameters const& params, Config const& cfg);

    UpgradeParameters const& getParameters() const;

        std::vector<LedgerUpgrade>
    createUpgradesFor(LedgerHeader const& header) const;

        static void applyTo(LedgerUpgrade const& upgrade, AbstractLedgerTxn& ltx);

        static std::string toString(LedgerUpgrade const& upgrade);

    enum class UpgradeValidity
    {
        VALID,
        XDR_INVALID,
        INVALID
    };

                        static UpgradeValidity isValidForApply(UpgradeType const& upgrade,
                                           LedgerUpgrade& lupgrade,
                                           LedgerHeader const& header,
                                           uint32_t maxLedgerVersion);

            bool isValid(UpgradeType const& upgrade, LedgerUpgradeType& upgradeType,
                 bool nomination, Config const& cfg,
                 LedgerHeader const& header) const;

            std::string toString() const;

        UpgradeParameters
    removeUpgrades(std::vector<UpgradeType>::const_iterator beginUpdates,
                   std::vector<UpgradeType>::const_iterator endUpdates,
                   bool& updated);

    static void dropAll(Database& db);

    static void storeUpgradeHistory(Database& db, uint32_t ledgerSeq,
                                    LedgerUpgrade const& upgrade,
                                    LedgerEntryChanges const& changes,
                                    int index);
    static void deleteOldEntries(Database& db, uint32_t ledgerSeq,
                                 uint32_t count);

  private:
    UpgradeParameters mParams;

    bool timeForUpgrade(uint64_t time) const;

            bool isValidForNomination(LedgerUpgrade const& upgrade,
                              LedgerHeader const& header) const;

    static void applyVersionUpgrade(AbstractLedgerTxn& ltx,
                                    uint32_t newVersion);

    static void applyReserveUpgrade(AbstractLedgerTxn& ltx,
                                    uint32_t newReserve);
};
}
