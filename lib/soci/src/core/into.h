
#ifndef SOCI_INTO_H_INCLUDED
#define SOCI_INTO_H_INCLUDED

#include "into-type.h"
#include "exchange-traits.h"
#include "type-conversion.h"
#include <cstddef>
#include <vector>

namespace soci
{


template <typename T>
details::into_type_ptr into(T & t)
{
    return details::do_into(t,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::into_type_ptr into(T & t, indicator & ind)
{
    return details::do_into(t, ind,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::into_type_ptr into(T & t, std::vector<indicator> & ind)
{
    return details::do_into(t, ind,
        typename details::exchange_traits<T>::type_family());
}

template <typename T>
details::into_type_ptr into(T & t, std::size_t bufSize)
{
    return details::into_type_ptr(new details::into_type<T>(t, bufSize));
}

} // namespace soci

#endif // SOCI_INTO_H_INCLUDED
