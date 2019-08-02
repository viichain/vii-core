
#ifndef SOCI_POSTGRESQL_COMMON_H_INCLUDED
#define SOCI_POSTGRESQL_COMMON_H_INCLUDED

#include "soci-postgresql.h"
#include <limits>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

namespace soci
{

namespace details
{

namespace postgresql
{

template <typename T>
T string_to_integer(char const * buf)
{
    long long t(0);
    int n(0);
    int const converted = std::sscanf(buf, "%" LL_FMT_FLAGS "d%n", &t, &n);
    if (converted == 1 && static_cast<std::size_t>(n) == std::strlen(buf))
    {
                
        const T max = (std::numeric_limits<T>::max)();
        const T min = (std::numeric_limits<T>::min)();
        if (t <= static_cast<long long>(max) &&
            t >= static_cast<long long>(min))
        {
            return static_cast<T>(t);
        }
        else
        {
                        throw soci_error("Cannot convert data.");
        }
    }
    else
    {
                
        if (buf[0] == 't' && buf[1] == '\0')
        {
            return static_cast<T>(1);
        }
        else if (buf[0] == 'f' && buf[1] == '\0')
        {
            return static_cast<T>(0);
        }
        else
        {
            throw soci_error("Cannot convert data.");
        }
    }
}

template <typename T>
T string_to_unsigned_integer(char const * buf)
{
    unsigned long long t(0);
    int n(0);
    int const converted = std::sscanf(buf, "%" LL_FMT_FLAGS "u%n", &t, &n);
    if (converted == 1 && static_cast<std::size_t>(n) == std::strlen(buf))
    {
                
        const T max = (std::numeric_limits<T>::max)();
        if (t <= static_cast<unsigned long long>(max))
        {
            return static_cast<T>(t);
        }
        else
        {
                        throw soci_error("Cannot convert data.");
        }
    }
    else
    {
                
        if (buf[0] == 't' && buf[1] == '\0')
        {
            return static_cast<T>(1);
        }
        else if (buf[0] == 'f' && buf[1] == '\0')
        {
            return static_cast<T>(0);
        }
        else
        {
            throw soci_error("Cannot convert data.");
        }
    }
}

double string_to_double(char const * buf);

void parse_std_tm(char const * buf, std::tm & t);

template <typename T>
std::size_t get_vector_size(void * p)
{
    std::vector<T> * v = static_cast<std::vector<T> *>(p);
    return v->size();
}

} // namespace postgresql

} // namespace details

} // namespace soci

#endif // SOCI_POSTGRESQL_COMMON_H_INCLUDED
