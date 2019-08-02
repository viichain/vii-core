
#ifndef SOCI_SQLITE3_COMMON_H_INCLUDED
#define SOCI_SQLITE3_COMMON_H_INCLUDED

#include <error.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <limits>

namespace soci { namespace details { namespace sqlite3 {

void parse_std_tm(char const *buf, std::tm &t);

template <typename T>
std::size_t get_vector_size(void *p)
{
    std::vector<T> *v = static_cast<std::vector<T> *>(p);
    return v->size();
}

template <typename T>
void resize_vector(void *p, std::size_t sz)
{
    std::vector<T> *v = static_cast<std::vector<T> *>(p);
    v->resize(sz);
}

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
    }

    throw soci_error("Cannot convert data.");
}

template <typename T>
T string_to_unsigned_integer(char const * buf)
{
    unsigned long long t(0);
    int n(0);
    int const converted = std::sscanf(buf, "%" LL_FMT_FLAGS "u%n", &t, &n);
    if (converted == 1 && static_cast<std::size_t>(n) == std::strlen(buf))
    {
                
        T const max = (std::numeric_limits<T>::max)();
        if (t <= static_cast<unsigned long long>(max))
        {
            return static_cast<T>(t);
        }
    }

    throw soci_error("Cannot convert data.");
}

}}} // namespace soci::details::sqlite3

#endif // SOCI_SQLITE3_COMMON_H_INCLUDED
