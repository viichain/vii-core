
#include "database/Database.h"
#include "crypto/Hex.h"
#include "database/DatabaseConnectionString.h"
#include "database/DatabaseTypeSpecificOperation.h"
#include "main/Application.h"
#include "main/Config.h"
#include "overlay/VIIXDR.h"
#include "util/GlobalChecks.h"
#include "util/Logging.h"
#include "util/Timer.h"
#include "util/types.h"

#include "bucket/BucketManager.h"
#include "herder/HerderPersistence.h"
#include "herder/Upgrades.h"
#include "history/HistoryManager.h"
#include "ledger/LedgerHeaderUtils.h"
#include "ledger/LedgerTxn.h"
#include "main/ExternalQueue.h"
#include "main/PersistentState.h"
#include "overlay/BanManager.h"
#include "overlay/OverlayManager.h"
#include "overlay/PeerManager.h"
#include "transactions/TransactionFrame.h"

#include "medida/counter.h"
#include "medida/metrics_registry.h"
#include "medida/timer.h"

#include <lib/soci/src/backends/sqlite3/soci-sqlite3.h>
#ifdef USE_POSTGRES
#include <lib/soci/src/backends/postgresql/soci-postgresql.h>
#endif
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" int
sqlite3_carray_init(sqlite_api::sqlite3* db, char** pzErrMsg,
                    const sqlite_api::sqlite3_api_routines* pApi);


namespace viichain
{

using namespace soci;
using namespace std;

bool Database::gDriversRegistered = false;

static unsigned long const SCHEMA_VERSION = 10;

static int const MIN_SQLITE_MAJOR_VERSION = 3;
static int const MIN_SQLITE_MINOR_VERSION = 26;
static int const MIN_SQLITE_VERSION =
    (1000000 * MIN_SQLITE_MAJOR_VERSION) + (1000 * MIN_SQLITE_MINOR_VERSION);

static int const MIN_POSTGRESQL_MAJOR_VERSION = 9;
static int const MIN_POSTGRESQL_MINOR_VERSION = 5;
static int const MIN_POSTGRESQL_VERSION =
    (10000 * MIN_POSTGRESQL_MAJOR_VERSION) +
    (100 * MIN_POSTGRESQL_MINOR_VERSION);

#ifdef USE_POSTGRES
static std::string
badPgVersion(int vers)
{
    std::ostringstream msg;
    int maj = (vers / 10000);
    int min = (vers - (maj * 10000)) / 100;
    msg << "PostgreSQL version " << maj << '.' << min
        << " is too old, must use at least " << MIN_POSTGRESQL_MAJOR_VERSION
        << '.' << MIN_POSTGRESQL_MINOR_VERSION;
    return msg.str();
}
#endif

static std::string
badSqliteVersion(int vers)
{
    std::ostringstream msg;
    int maj = (vers / 1000000);
    int min = (vers - (maj * 1000000)) / 1000;
    msg << "SQLite version " << maj << '.' << min
        << " is too old, must use at least " << MIN_SQLITE_MAJOR_VERSION << '.'
        << MIN_SQLITE_MINOR_VERSION;
    return msg.str();
}

void
Database::registerDrivers()
{
    if (!gDriversRegistered)
    {
        register_factory_sqlite3();
#ifdef USE_POSTGRES
        register_factory_postgresql();
#endif
        gDriversRegistered = true;
    }
}

class DatabaseConfigureSessionOp : public DatabaseTypeSpecificOperation<void>
{
    soci::session& mSession;

  public:
    DatabaseConfigureSessionOp(soci::session& sess) : mSession(sess)
    {
    }
    void
    doSqliteSpecificOperation(soci::sqlite3_session_backend* sq) override
    {
        int vers = sqlite_api::sqlite3_libversion_number();
        if (vers < MIN_SQLITE_VERSION)
        {
            throw std::runtime_error(badSqliteVersion(vers));
        }

        mSession << "PRAGMA journal_mode = WAL";
                        mSession << "PRAGMA busy_timeout = 10000";

                sqlite3_carray_init(sq->conn_, nullptr, nullptr);
    }
#ifdef USE_POSTGRES
    void
    doPostgresSpecificOperation(soci::postgresql_session_backend* pg) override
    {
        int vers = PQserverVersion(pg->conn_);
        if (vers < MIN_POSTGRESQL_VERSION)
        {
            throw std::runtime_error(badPgVersion(vers));
        }
        mSession
            << "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL "
               "SERIALIZABLE";
    }
#endif
};

Database::Database(Application& app)
    : mApp(app)
    , mQueryMeter(
          app.getMetrics().NewMeter({"database", "query", "exec"}, "query"))
    , mStatementsSize(
          app.getMetrics().NewCounter({"database", "memory", "statements"}))
    , mExcludedQueryTime(0)
    , mExcludedTotalTime(0)
    , mLastIdleQueryTime(0)
    , mLastIdleTotalTime(app.getClock().now())
{
    registerDrivers();

    CLOG(INFO, "Database") << "Connecting to: "
                           << removePasswordFromConnectionString(
                                  app.getConfig().DATABASE.value);
    mSession.open(app.getConfig().DATABASE.value);
    DatabaseConfigureSessionOp op(mSession);
    doDatabaseTypeSpecificOperation(op);
}

void
Database::applySchemaUpgrade(unsigned long vers)
{
    clearPreparedStatementCache();

    soci::transaction tx(mSession);
    switch (vers)
    {
    case 7:
        Upgrades::dropAll(*this);
        mSession << "ALTER TABLE accounts ADD buyingliabilities BIGINT "
                    "CHECK (buyingliabilities >= 0)";
        mSession << "ALTER TABLE accounts ADD sellingliabilities BIGINT "
                    "CHECK (sellingliabilities >= 0)";
        mSession << "ALTER TABLE trustlines ADD buyingliabilities BIGINT "
                    "CHECK (buyingliabilities >= 0)";
        mSession << "ALTER TABLE trustlines ADD sellingliabilities BIGINT "
                    "CHECK (sellingliabilities >= 0)";
        break;

    case 8:
        mSession << "ALTER TABLE peers RENAME flags TO type";
        mSession << "UPDATE peers SET type = 2*type";
        break;
    case 9:
                mSession << "ALTER TABLE accounts ADD signers TEXT";
        mApp.getLedgerTxnRoot().writeSignersTableIntoAccountsTable();
        mSession << "DROP TABLE IF EXISTS signers";

                mApp.getLedgerTxnRoot().encodeDataNamesBase64();
        mApp.getLedgerTxnRoot().encodeHomeDomainsBase64();

                mApp.getLedgerTxnRoot().writeOffersIntoSimplifiedOffersTable();
        break;
    case 10:
                mApp.getHerderPersistence().createQuorumTrackingTable(mSession);
        break;
    default:
        if (vers <= 6)
        {
            throw std::runtime_error(
                "Database version is too old, must use at least 7");
        }
        else
        {
            throw std::runtime_error("Unknown DB schema version");
        }
    }
    tx.commit();
}

void
Database::upgradeToCurrentSchema()
{
    auto vers = getDBSchemaVersion();
    if (vers > SCHEMA_VERSION)
    {
        std::string s = ("DB schema version " + std::to_string(vers) +
                         " is newer than application schema " +
                         std::to_string(SCHEMA_VERSION));
        throw std::runtime_error(s);
    }
    while (vers < SCHEMA_VERSION)
    {
        ++vers;
        CLOG(INFO, "Database")
            << "Applying DB schema upgrade to version " << vers;
        applySchemaUpgrade(vers);
        putSchemaVersion(vers);
    }
    CLOG(INFO, "Database") << "DB schema is in current version";
    assert(vers == SCHEMA_VERSION);
}

void
Database::putSchemaVersion(unsigned long vers)
{
    mApp.getPersistentState().setState(PersistentState::kDatabaseSchema,
                                       std::to_string(vers));
}

unsigned long
Database::getDBSchemaVersion()
{
    unsigned long vers = 0;
    try
    {
        auto vstr = mApp.getPersistentState().getState(
            PersistentState::kDatabaseSchema);
        vers = std::stoul(vstr);
    }
    catch (...)
    {
    }
    if (vers == 0)
    {
        throw std::runtime_error(
            "No DB schema version found, try vii-core new-db");
    }
    return vers;
}

unsigned long
Database::getAppSchemaVersion()
{
    return SCHEMA_VERSION;
}

medida::TimerContext
Database::getInsertTimer(std::string const& entityName)
{
    mEntityTypes.insert(entityName);
    mQueryMeter.Mark();
    return mApp.getMetrics()
        .NewTimer({"database", "insert", entityName})
        .TimeScope();
}

medida::TimerContext
Database::getSelectTimer(std::string const& entityName)
{
    mEntityTypes.insert(entityName);
    mQueryMeter.Mark();
    return mApp.getMetrics()
        .NewTimer({"database", "select", entityName})
        .TimeScope();
}

medida::TimerContext
Database::getDeleteTimer(std::string const& entityName)
{
    mEntityTypes.insert(entityName);
    mQueryMeter.Mark();
    return mApp.getMetrics()
        .NewTimer({"database", "delete", entityName})
        .TimeScope();
}

medida::TimerContext
Database::getUpdateTimer(std::string const& entityName)
{
    mEntityTypes.insert(entityName);
    mQueryMeter.Mark();
    return mApp.getMetrics()
        .NewTimer({"database", "update", entityName})
        .TimeScope();
}

medida::TimerContext
Database::getUpsertTimer(std::string const& entityName)
{
    mEntityTypes.insert(entityName);
    mQueryMeter.Mark();
    return mApp.getMetrics()
        .NewTimer({"database", "upsert", entityName})
        .TimeScope();
}

void
Database::setCurrentTransactionReadOnly()
{
    if (!isSqlite())
    {
        auto prep = getPreparedStatement("SET TRANSACTION READ ONLY");
        auto& st = prep.statement();
        st.define_and_bind();
        st.execute(false);
    }
}

bool
Database::isSqlite() const
{
    return mApp.getConfig().DATABASE.value.find("sqlite3:") !=
           std::string::npos;
}

bool
Database::canUsePool() const
{
    return !(mApp.getConfig().DATABASE.value == ("sqlite3://:memory:"));
}

void
Database::clearPreparedStatementCache()
{
            for (auto st : mStatements)
    {
        st.second->clean_up(true);
    }
    mStatements.clear();
    mStatementsSize.set_count(mStatements.size());
}

void
Database::initialize()
{
    clearPreparedStatementCache();
        
            mApp.getLedgerTxnRoot().dropAccounts();
    mApp.getLedgerTxnRoot().dropOffers();
    mApp.getLedgerTxnRoot().dropTrustLines();
    OverlayManager::dropAll(*this);
    PersistentState::dropAll(*this);
    ExternalQueue::dropAll(*this);
    LedgerHeaderUtils::dropAll(*this);
    TransactionFrame::dropAll(*this);
    HistoryManager::dropAll(*this);
    mApp.getBucketManager().dropAll();
    HerderPersistence::dropAll(*this);
    mApp.getLedgerTxnRoot().dropData();
    BanManager::dropAll(*this);
    putSchemaVersion(6);

    LOG(INFO) << "* ";
    LOG(INFO) << "* The database has been initialized";
    LOG(INFO) << "* ";
}

soci::session&
Database::getSession()
{
        assertThreadIsMain();
    return mSession;
}

soci::connection_pool&
Database::getPool()
{
    if (!mPool)
    {
        auto const& c = mApp.getConfig().DATABASE;
        if (!canUsePool())
        {
            std::string s("Can't create connection pool to ");
            s += removePasswordFromConnectionString(c.value);
            throw std::runtime_error(s);
        }
        size_t n = std::thread::hardware_concurrency();
        LOG(INFO) << "Establishing " << n << "-entry connection pool to: "
                  << removePasswordFromConnectionString(c.value);
        mPool = std::make_unique<soci::connection_pool>(n);
        for (size_t i = 0; i < n; ++i)
        {
            LOG(DEBUG) << "Opening pool entry " << i;
            soci::session& sess = mPool->at(i);
            sess.open(c.value);
            DatabaseConfigureSessionOp op(sess);
            doDatabaseTypeSpecificOperation(op);
        }
    }
    assert(mPool);
    return *mPool;
}

class SQLLogContext : NonCopyable
{
    std::string mName;
    soci::session& mSess;
    std::ostringstream mCapture;

  public:
    SQLLogContext(std::string const& name, soci::session& sess)
        : mName(name), mSess(sess)
    {
        mSess.set_log_stream(&mCapture);
    }
    ~SQLLogContext()
    {
        mSess.set_log_stream(nullptr);
        std::string captured = mCapture.str();
        std::istringstream rd(captured);
        std::string buf;
        CLOG(INFO, "Database") << "";
        CLOG(INFO, "Database") << "";
        CLOG(INFO, "Database") << "[SQL] -----------------------";
        CLOG(INFO, "Database") << "[SQL] begin capture: " << mName;
        CLOG(INFO, "Database") << "[SQL] -----------------------";
        while (std::getline(rd, buf))
        {
            CLOG(INFO, "Database") << "[SQL:" << mName << "] " << buf;
            buf.clear();
        }
        CLOG(INFO, "Database") << "[SQL] -----------------------";
        CLOG(INFO, "Database") << "[SQL] end capture: " << mName;
        CLOG(INFO, "Database") << "[SQL] -----------------------";
        CLOG(INFO, "Database") << "";
        CLOG(INFO, "Database") << "";
    }
};

StatementContext
Database::getPreparedStatement(std::string const& query)
{
    auto i = mStatements.find(query);
    std::shared_ptr<soci::statement> p;
    if (i == mStatements.end())
    {
        p = std::make_shared<soci::statement>(mSession);
        p->alloc();
        p->prepare(query);
        mStatements.insert(std::make_pair(query, p));
        mStatementsSize.set_count(mStatements.size());
    }
    else
    {
        p = i->second;
    }
    StatementContext sc(p);
    return sc;
}

std::shared_ptr<SQLLogContext>
Database::captureAndLogSQL(std::string contextName)
{
    return make_shared<SQLLogContext>(contextName, mSession);
}

medida::Meter&
Database::getQueryMeter()
{
    return mQueryMeter;
}

std::chrono::nanoseconds
Database::totalQueryTime() const
{
    std::vector<std::string> qtypes = {"insert", "delete", "select", "update"};
    std::chrono::nanoseconds nsq(0);
    for (auto const& q : qtypes)
    {
        for (auto const& e : mEntityTypes)
        {
            auto& timer = mApp.getMetrics().NewTimer({"database", q, e});
            uint64_t sumns = static_cast<uint64_t>(
                timer.sum() *
                static_cast<double>(timer.duration_unit().count()));
            nsq += std::chrono::nanoseconds(sumns);
        }
    }
    return nsq;
}

void
Database::excludeTime(std::chrono::nanoseconds const& queryTime,
                      std::chrono::nanoseconds const& totalTime)
{
    mExcludedQueryTime += queryTime;
    mExcludedTotalTime += totalTime;
}

uint32_t
Database::recentIdleDbPercent()
{
    std::chrono::nanoseconds query = totalQueryTime();
    query -= mLastIdleQueryTime;
    query -= mExcludedQueryTime;

    std::chrono::nanoseconds total = mApp.getClock().now() - mLastIdleTotalTime;
    total -= mExcludedTotalTime;

    if (total == std::chrono::nanoseconds::zero())
    {
        return 100;
    }

    uint32_t queryPercent =
        static_cast<uint32_t>((100 * query.count()) / total.count());
    uint32_t idlePercent = 100 - queryPercent;
    if (idlePercent > 100)
    {
                CLOG(WARNING, "Database") << "DB idle percent (" << idlePercent
                                  << ") over 100, limiting to 100";
        idlePercent = 100;
    }

    CLOG(DEBUG, "Database") << "Estimated DB idle: " << idlePercent << "%"
                            << " (query=" << query.count() << "ns"
                            << ", total=" << total.count() << "ns)";

    mLastIdleQueryTime = totalQueryTime();
    mLastIdleTotalTime = mApp.getClock().now();
    mExcludedQueryTime = std::chrono::nanoseconds(0);
    mExcludedTotalTime = std::chrono::nanoseconds(0);
    return idlePercent;
}

DBTimeExcluder::DBTimeExcluder(Application& app)
    : mApp(app)
    , mStartQueryTime(app.getDatabase().totalQueryTime())
    , mStartTotalTime(app.getClock().now())
{
}

DBTimeExcluder::~DBTimeExcluder()
{
    auto deltaQ = mApp.getDatabase().totalQueryTime() - mStartQueryTime;
    auto deltaT = mApp.getClock().now() - mStartTotalTime;
    mApp.getDatabase().excludeTime(deltaQ, deltaT);
}
}
