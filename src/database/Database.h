#pragma once


#include "database/DatabaseTypeSpecificOperation.h"
#include "medida/timer_context.h"
#include "overlay/StellarXDR.h"
#include "util/NonCopyable.h"
#include "util/Timer.h"
#include <set>
#include <soci.h>
#include <string>

namespace medida
{
class Meter;
class Counter;
}

namespace viichain
{
class Application;
class SQLLogContext;

class StatementContext : NonCopyable
{
    std::shared_ptr<soci::statement> mStmt;

  public:
    StatementContext(std::shared_ptr<soci::statement> stmt) : mStmt(stmt)
    {
        mStmt->clean_up(false);
    }
    StatementContext(StatementContext&& other)
    {
        mStmt = other.mStmt;
        other.mStmt.reset();
    }
    ~StatementContext()
    {
        if (mStmt)
        {
            mStmt->clean_up(false);
        }
    }
    soci::statement&
    statement()
    {
        return *mStmt;
    }
};

class Database : NonMovableOrCopyable
{
    Application& mApp;
    medida::Meter& mQueryMeter;
    soci::session mSession;
    std::unique_ptr<soci::connection_pool> mPool;

    std::map<std::string, std::shared_ptr<soci::statement>> mStatements;
    medida::Counter& mStatementsSize;

            std::set<std::string> mEntityTypes;
    std::chrono::nanoseconds mExcludedQueryTime;
    std::chrono::nanoseconds mExcludedTotalTime;
    std::chrono::nanoseconds mLastIdleQueryTime;
    VirtualClock::time_point mLastIdleTotalTime;

    static bool gDriversRegistered;
    static void registerDrivers();
    void applySchemaUpgrade(unsigned long vers);

  public:
            Database(Application& app);

            medida::Meter& getQueryMeter();

                std::chrono::nanoseconds totalQueryTime() const;

            void excludeTime(std::chrono::nanoseconds const& queryTime,
                     std::chrono::nanoseconds const& totalTime);

                uint32_t recentIdleDbPercent();

                std::shared_ptr<SQLLogContext> captureAndLogSQL(std::string contextName);

                    StatementContext getPreparedStatement(std::string const& query);

            void clearPreparedStatementCache();

                medida::TimerContext getInsertTimer(std::string const& entityName);
    medida::TimerContext getSelectTimer(std::string const& entityName);
    medida::TimerContext getDeleteTimer(std::string const& entityName);
    medida::TimerContext getUpdateTimer(std::string const& entityName);
    medida::TimerContext getUpsertTimer(std::string const& entityName);

                void setCurrentTransactionReadOnly();

        bool isSqlite() const;

        template <typename T>
    T doDatabaseTypeSpecificOperation(DatabaseTypeSpecificOperation<T>& op);

            bool canUsePool() const;

            void initialize();

        void putSchemaVersion(unsigned long vers);

        unsigned long getDBSchemaVersion();

        unsigned long getAppSchemaVersion();

        void upgradeToCurrentSchema();

        soci::session& getSession();

            soci::connection_pool& getPool();
};

template <typename T>
T
Database::doDatabaseTypeSpecificOperation(DatabaseTypeSpecificOperation<T>& op)
{
    auto b = mSession.get_backend();
    if (auto sq = dynamic_cast<soci::sqlite3_session_backend*>(b))
    {
        return op.doSqliteSpecificOperation(sq);
#ifdef USE_POSTGRES
    }
    else if (auto pg = dynamic_cast<soci::postgresql_session_backend*>(b))
    {
        return op.doPostgresSpecificOperation(pg);
#endif
    }
    else
    {
                abort();
    }
}

class DBTimeExcluder : NonCopyable
{
    Application& mApp;
    std::chrono::nanoseconds mStartQueryTime;
    VirtualClock::time_point mStartTotalTime;

  public:
    DBTimeExcluder(Application& mApp);
    ~DBTimeExcluder();
};
}
