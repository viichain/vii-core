
#ifndef SOCI_USE_H_INCLUDED
#define SOCI_USE_H_INCLUDED

#include "use-type.h"
#include "exchange-traits.h"
#include "type-conversion.h"

namespace soci
{


template <typename T>
details::use_type_ptr use(T & t, std::string const & name = std::string())
{
    return details::do_use(t, name,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::use_type_ptr use(T const & t,
    std::string const & name = std::string())
{
    return details::do_use(t, name,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::use_type_ptr use(T & t, indicator & ind,
    std::string const &name = std::string())
{
    return details::do_use(t, ind, name,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::use_type_ptr use(T const & t, indicator & ind,
    std::string const &name = std::string())
{
    return details::do_use(t, ind, name,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::use_type_ptr use(T & t, std::vector<indicator> & ind,
    std::string const & name = std::string())
{
    return details::do_use(t, ind, name,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::use_type_ptr use(T const & t, std::vector<indicator> & ind,
    std::string const & name = std::string())
{
    return details::do_use(t, ind, name,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::use_type_ptr use(T & t, std::size_t bufSize,
    std::string const & name = std::string())
{
    return details::use_type_ptr(new details::use_type<T>(t, bufSize));
}

template <typename T>
details::use_type_ptr use(T const & t, std::size_t bufSize,
    std::string const & name = std::string())
{
    return details::use_type_ptr(new details::use_type<T>(t, bufSize));
}

} // namespace soci

#endif // SOCI_USE_H_INCLUDED
