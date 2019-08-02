
#include "util/Thread.h"
#include "util/Logging.h"

#ifdef _WIN32
#else
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace viichain
{

#if defined(_WIN32)

void
runCurrentThreadWithLowPriority()
{
    HANDLE curThread = ::GetCurrentThread();
    BOOL ret = ::SetThreadPriority(curThread, THREAD_PRIORITY_BELOW_NORMAL);

    if (!ret)
    {
        LOG(DEBUG) << "Unable to set priority for thread: " << ret;
    }
}

#elif defined(__linux__)

void
runCurrentThreadWithLowPriority()
{
    constexpr auto const LOW_PRIORITY_NICE = 5;

    auto newNice = nice(LOW_PRIORITY_NICE);
    if (newNice != LOW_PRIORITY_NICE)
    {
        LOG(DEBUG) << "Unable to run worker thread with low priority. "
                      "Normal priority will be used.";
    }
}

#elif defined(__APPLE__)

void
runCurrentThreadWithLowPriority()
{
                    constexpr auto const LOW_PRIORITY_NICE = 5;
    struct sched_param sp;
    int policy;
    int ret = pthread_getschedparam(pthread_self(), &policy, &sp);
    if (ret != 0)
    {
        LOG(DEBUG) << "Unable to get priority for thread: " << ret;
    }
    sp.sched_priority -= LOW_PRIORITY_NICE;
    ret = pthread_setschedparam(pthread_self(), policy, &sp);
    if (ret != 0)
    {
        LOG(DEBUG) << "Unable to set priority for thread: " << ret;
    }
}

#else

void
runCurrentThreadWithLowPriority()
{
}

#endif
}
