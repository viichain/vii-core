
#define SOCI_SOURCE
#include "connection-pool.h"
#include "error.h"
#include "session.h"
#include <vector>
#include <utility>

#ifndef _WIN32

#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

using namespace soci;

struct connection_pool::connection_pool_impl
{
    bool find_free(std::size_t & pos)
    {
        for (std::size_t i = 0; i != sessions_.size(); ++i)
        {
            if (sessions_[i].first)
            {
                pos = i;
                return true;
            }
        }

        return false;
    }

        std::vector<std::pair<bool, session *> > sessions_;
    pthread_mutex_t mtx_;
    pthread_cond_t cond_;
};

connection_pool::connection_pool(std::size_t size)
{
    if (size == 0)
    {
        throw soci_error("Invalid pool size");
    }

    pimpl_ = new connection_pool_impl();
    pimpl_->sessions_.resize(size);
    for (std::size_t i = 0; i != size; ++i)
    {
        pimpl_->sessions_[i] = std::make_pair(true, new session());
    }

    int cc = pthread_mutex_init(&(pimpl_->mtx_), NULL);
    if (cc != 0)
    {
        throw soci_error("Synchronization error");
    }

    cc = pthread_cond_init(&(pimpl_->cond_), NULL);
    if (cc != 0)
    {
        throw soci_error("Synchronization error");
    }
}

connection_pool::~connection_pool()
{
    for (std::size_t i = 0; i != pimpl_->sessions_.size(); ++i)
    {
        delete pimpl_->sessions_[i].second;
    }

    pthread_mutex_destroy(&(pimpl_->mtx_));
    pthread_cond_destroy(&(pimpl_->cond_));

    delete pimpl_;
}

session & connection_pool::at(std::size_t pos)
{
    if (pos >= pimpl_->sessions_.size())
    {
        throw soci_error("Invalid pool position");
    }

    return *(pimpl_->sessions_[pos].second);
}

std::size_t connection_pool::lease()
{
    std::size_t pos;

        bool const success = try_lease(pos, -1);
    assert(success);

    return pos;
}

bool connection_pool::try_lease(std::size_t & pos, int timeout)
{
    struct timespec tm;
    if (timeout >= 0)
    {
        
        struct timeval tmv;
        gettimeofday(&tmv, NULL);

        tm.tv_sec = tmv.tv_sec + timeout / 1000;
        tm.tv_nsec = tmv.tv_usec * 1000 + (timeout % 1000) * 1000 * 1000;
    }

    int cc = pthread_mutex_lock(&(pimpl_->mtx_));
    if (cc != 0)
    {
        throw soci_error("Synchronization error");
    }

    while (pimpl_->find_free(pos) == false)
    {
        if (timeout < 0)
        {
                        cc = pthread_cond_wait(&(pimpl_->cond_), &(pimpl_->mtx_));
        }
        else
        {
                        cc = pthread_cond_timedwait(
                &(pimpl_->cond_), &(pimpl_->mtx_), &tm);
        }

        if (cc == ETIMEDOUT)
        {
            break;
        }
    }

    if (cc == 0)
    {
        pimpl_->sessions_[pos].first = false;
    }

    pthread_mutex_unlock(&(pimpl_->mtx_));

    return cc == 0;
}

void connection_pool::give_back(std::size_t pos)
{
    if (pos >= pimpl_->sessions_.size())
    {
        throw soci_error("Invalid pool position");
    }

    int cc = pthread_mutex_lock(&(pimpl_->mtx_));
    if (cc != 0)
    {
        throw soci_error("Synchronization error");
    }

    if (pimpl_->sessions_[pos].first)
    {
        pthread_mutex_unlock(&(pimpl_->mtx_));
        throw soci_error("Cannot release pool entry (already free)");
    }

    pimpl_->sessions_[pos].first = true;

    pthread_mutex_unlock(&(pimpl_->mtx_));

    pthread_cond_signal(&(pimpl_->cond_));
}

#else

#include <Windows.h>

using namespace soci;

struct connection_pool::connection_pool_impl
{
    bool find_free(std::size_t & pos)
    {
        for (std::size_t i = 0; i != sessions_.size(); ++i)
        {
            if (sessions_[i].first)
            {
                pos = i;
                return true;
            }
        }

        return false;
    }

        std::vector<std::pair<bool, session *> > sessions_;

    CRITICAL_SECTION mtx_;
    HANDLE sem_;
};

connection_pool::connection_pool(std::size_t size)
{
    if (size == 0)
    {
        throw soci_error("Invalid pool size");
    }

    pimpl_ = new connection_pool_impl();
    pimpl_->sessions_.resize(size);
    for (std::size_t i = 0; i != size; ++i)
    {
        pimpl_->sessions_[i] = std::make_pair(true, new session());
    }

    InitializeCriticalSection(&(pimpl_->mtx_));

        HANDLE s = CreateSemaphore(NULL,
        static_cast<LONG>(size), static_cast<LONG>(size), NULL);
    if (s == NULL)
    {
        throw soci_error("Synchronization error");
    }

    pimpl_->sem_ = s;
}

connection_pool::~connection_pool()
{
    for (std::size_t i = 0; i != pimpl_->sessions_.size(); ++i)
    {
        delete pimpl_->sessions_[i].second;
    }

    DeleteCriticalSection(&(pimpl_->mtx_));
    CloseHandle(pimpl_->sem_);

    delete pimpl_;
}

session & connection_pool::at(std::size_t pos)
{
    if (pos >= pimpl_->sessions_.size())
    {
        throw soci_error("Invalid pool position");
    }

    return *(pimpl_->sessions_[pos].second);
}

std::size_t connection_pool::lease()
{
    std::size_t pos;

        bool const success = try_lease(pos, -1);
    assert(success);    
    if (!success)
    {
            }

    return pos;
}

bool connection_pool::try_lease(std::size_t & pos, int timeout)
{
    DWORD cc = WaitForSingleObject(pimpl_->sem_,
        timeout >= 0 ? static_cast<DWORD>(timeout) : INFINITE);
    if (cc == WAIT_OBJECT_0)
    {
        
        EnterCriticalSection(&(pimpl_->mtx_));

        bool const success = pimpl_->find_free(pos);
        assert(success);
        if (!success)
        {
                    }

        pimpl_->sessions_[pos].first = false;

        LeaveCriticalSection(&(pimpl_->mtx_));

        return true;
    }
    else if (cc == WAIT_TIMEOUT)
    {
        return false;
    }
    else
    {
        throw soci_error("Synchronization error");
    }
}

void connection_pool::give_back(std::size_t pos)
{
    if (pos >= pimpl_->sessions_.size())
    {
        throw soci_error("Invalid pool position");
    }

    EnterCriticalSection(&(pimpl_->mtx_));

    if (pimpl_->sessions_[pos].first)
    {
        LeaveCriticalSection(&(pimpl_->mtx_));
        throw soci_error("Cannot release pool entry (already free)");
    }

    pimpl_->sessions_[pos].first = true;

    LeaveCriticalSection(&(pimpl_->mtx_));

    ReleaseSemaphore(pimpl_->sem_, 1, NULL);
}

#endif // _WIN32
