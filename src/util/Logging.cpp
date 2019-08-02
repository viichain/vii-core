
#include "util/Logging.h"
#include "main/Application.h"
#include "util/types.h"
#include <list>

namespace viichain
{

std::array<std::string const, 13> const Logging::kPartitionNames = {
    "Fs",      "SCP",    "Bucket", "Database", "History", "Process",  "Ledger",
    "Overlay", "Herder", "Tx",     "LoadGen",  "Work",    "Invariant"};

el::Configurations Logging::gDefaultConf;

template <typename T> class LockElObject : public NonMovableOrCopyable
{
    T* const mItem;

  public:
    explicit LockElObject(T* elObj) : mItem{elObj}
    {
        assert(mItem);
        static_assert(std::is_base_of<el::base::threading::ThreadSafe,
                                      std::remove_cv_t<T>>::value,
                      "ThreadSafe (easylogging) param required");
        mItem->acquireLock();
    }

    ~LockElObject()
    {
        mItem->releaseLock();
    }
};

class LockHelper
{
                std::unique_ptr<LockElObject<el::base::RegisteredLoggers>>
        mRegisteredLoggersLock;
    std::list<LockElObject<el::Logger>> mLoggersLocks;

  public:
    explicit LockHelper(std::vector<std::string> const& loggers)
    {
        mRegisteredLoggersLock =
            std::make_unique<LockElObject<el::base::RegisteredLoggers>>(
                el::Helpers::storage()->registeredLoggers());
        for (auto const& logger : loggers)
        {
                        auto l = el::Loggers::getLogger(logger, false);
            if (l)
            {
                mLoggersLocks.emplace_back(l);
            }
        }
    }
};

void
Logging::setFmt(std::string const& peerID, bool timestamps)
{
    std::string datetime;
    if (timestamps)
    {
        datetime = "%datetime{%Y-%M-%dT%H:%m:%s.%g}";
    }
    const std::string shortFmt =
        datetime + " " + peerID + " [%logger %level] %msg";
    const std::string longFmt = shortFmt + " [%fbase:%line]";

    gDefaultConf.setGlobally(el::ConfigurationType::Format, shortFmt);
    gDefaultConf.set(el::Level::Error, el::ConfigurationType::Format, longFmt);
    gDefaultConf.set(el::Level::Trace, el::ConfigurationType::Format, longFmt);
    gDefaultConf.set(el::Level::Fatal, el::ConfigurationType::Format, longFmt);
    el::Loggers::reconfigureAllLoggers(gDefaultConf);
}

void
Logging::init()
{
        el::Loggers::addFlag(el::LoggingFlag::DisableApplicationAbortOnFatalLog);

    for (auto const& logger : kPartitionNames)
    {
        el::Loggers::getLogger(logger);
    }

    gDefaultConf.setToDefault();
    gDefaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
    gDefaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
    setFmt("<startup>");
}

void
Logging::setLoggingToFile(std::string const& filename)
{
    gDefaultConf.setGlobally(el::ConfigurationType::ToFile, "true");
    gDefaultConf.setGlobally(el::ConfigurationType::Filename, filename);
    el::Loggers::reconfigureAllLoggers(gDefaultConf);
}

el::Level
Logging::getLogLevel(std::string const& partition)
{
    el::Logger* logger = el::Loggers::getLogger(partition);
    if (logger->typedConfigurations()->enabled(el::Level::Trace))
        return el::Level::Trace;
    if (logger->typedConfigurations()->enabled(el::Level::Debug))
        return el::Level::Debug;
    if (logger->typedConfigurations()->enabled(el::Level::Info))
        return el::Level::Info;
    if (logger->typedConfigurations()->enabled(el::Level::Warning))
        return el::Level::Warning;
    if (logger->typedConfigurations()->enabled(el::Level::Error))
        return el::Level::Error;
    if (logger->typedConfigurations()->enabled(el::Level::Fatal))
        return el::Level::Fatal;
    return el::Level::Unknown;
}

bool
Logging::logDebug(std::string const& partition)
{
    auto lev = Logging::getLogLevel(partition);
    return lev == el::Level::Debug || lev == el::Level::Trace;
}

bool
Logging::logTrace(std::string const& partition)
{
    auto lev = Logging::getLogLevel(partition);
    return lev == el::Level::Trace;
}

void
Logging::setLogLevel(el::Level level, const char* partition)
{
    el::Configurations config = gDefaultConf;

    if (level == el::Level::Debug)
        config.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
    else if (level == el::Level::Info)
    {
        config.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
    }
    else if (level == el::Level::Warning)
    {
        config.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
    }
    else if (level == el::Level::Error)
    {
        config.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Warning, el::ConfigurationType::Enabled, "false");
    }
    else if (level == el::Level::Fatal)
    {
        config.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Warning, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Error, el::ConfigurationType::Enabled, "false");
    }
    else if (level == el::Level::Unknown)
    {
        config.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Warning, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Error, el::ConfigurationType::Enabled, "false");
        config.set(el::Level::Fatal, el::ConfigurationType::Enabled, "false");
    }

    if (partition)
        el::Loggers::reconfigureLogger(partition, config);
    else
        el::Loggers::reconfigureAllLoggers(config);
}

std::string
Logging::getStringFromLL(el::Level level)
{
    switch (level)
    {
    case el::Level::Global:
        return "Global";
    case el::Level::Trace:
        return "Trace";
    case el::Level::Debug:
        return "Debug";
    case el::Level::Fatal:
        return "Fatal";
    case el::Level::Error:
        return "Error";
    case el::Level::Warning:
        return "Warning";
    case el::Level::Verbose:
        return "Verbose";
    case el::Level::Info:
        return "Info";
    case el::Level::Unknown:
        return "Unknown";
    }
    return "????";
}

el::Level
Logging::getLLfromString(std::string const& levelName)
{
    if (iequals(levelName, "trace"))
    {
        return el::Level::Trace;
    }

    if (iequals(levelName, "debug"))
    {
        return el::Level::Debug;
    }

    if (iequals(levelName, "warning"))
    {
        return el::Level::Warning;
    }

    if (iequals(levelName, "fatal"))
    {
        return el::Level::Fatal;
    }

    if (iequals(levelName, "error"))
    {
        return el::Level::Error;
    }

    if (iequals(levelName, "none"))
    {
        return el::Level::Unknown;
    }

    return el::Level::Info;
}

void
Logging::rotate()
{
    std::vector<std::string> loggers(kPartitionNames.begin(),
                                     kPartitionNames.end());
    loggers.insert(loggers.begin(), "default");

            LockHelper lock{loggers};

    for (auto const& logger : loggers)
    {
        auto loggerObj = el::Loggers::getLogger(logger);

                        
                                        auto prevMaxFileSize =
            loggerObj->typedConfigurations()->maxLogFileSize(el::Level::Global);

                                        el::Loggers::reconfigureLogger(
            logger, el::ConfigurationType::MaxLogFileSize, "1");

                                el::Loggers::reconfigureLogger(logger,
                                       el::ConfigurationType::MaxLogFileSize,
                                       std::to_string(prevMaxFileSize));
    }
}

std::string
Logging::normalizePartition(std::string const& partition)
{
    for (auto& p : kPartitionNames)
    {
        if (iequals(partition, p))
        {
            return p;
        }
    }
    throw std::invalid_argument("not a valid partition");
}
}
