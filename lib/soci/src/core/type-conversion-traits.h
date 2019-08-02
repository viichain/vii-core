
#ifndef SOCI_TYPE_CONVERSION_TRAITS_H_INCLUDED
#define SOCI_TYPE_CONVERSION_TRAITS_H_INCLUDED

#include "soci-backend.h"

namespace soci
{

template <typename T, typename Enable = void>
struct type_conversion
{
    typedef T base_type;

    static void from_base(base_type const & in, indicator ind, T & out)
    {
        if (ind == i_null)
        {
            throw soci_error("Null value not allowed for this type");
        }
        out = in;
    }

    static void to_base(T const & in, base_type & out, indicator & ind)
    {
        out = in;
        ind = i_ok;
    }
};

} // namespace soci

#endif // SOCI_TYPE_CONVERSION_TRAITS_H_INCLUDED
