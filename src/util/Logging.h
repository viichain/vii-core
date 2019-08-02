#pragma once


#define ELPP_THREAD_SAFE
#define ELPP_DISABLE_DEFAULT_CRASH_HANDLING
#define ELPP_NO_DEFAULT_LOG_FILE
#define ELPP_NO_CHECK_MACROS
#define ELPP_NO_DEBUG_MACROS
#define ELPP_DISABLE_PERFORMANCE_TRACKING
#define ELPP_WINSOCK2
#define ELPP_DEBUG_ERRORS

#include "lib/util/easylogging++.h"

namespace viichain
{
class Logging
{
    static el::Configurations gDefaultConf;

  public:
    static void init();
    static void setFmt(std::string const& peerID, bool timestamps = true);
    static void setLoggingToFile(std::string const& filename);
    static void setLogLevel(el::Level level, const char* partition);
    static el::Level getLLfromString(std::string const& levelName);
    static el::Level getLogLevel(std::string const& partition);
    static std::string getStringFromLL(el::Level);
    static bool logDebug(std::string const& partition);
    static bool logTrace(std::string const& partition);
    static void rotate();
        static std::string normalizePartition(std::string const& partition);

    static std::array<std::string const, 13> const kPartitionNames;
};
}
