
#ifndef SOCI_INTO_ROW_H_INCLUDED
#define SOCI_INTO_ROW_H_INCLUDED

#include "into-type.h"
#include "exchange-traits.h"
#include "row.h"
#include "statement.h"
#include <cstddef>

namespace soci
{

namespace details
{


template <>
class into_type<row>
    : public into_type_base // bypass the standard_into_type
{
public:
    into_type(row & r) : r_(r) {}
    into_type(row & r, indicator &) : r_(r) {}

private:
        virtual void define(statement_impl & st, int & /* position */)
    {
        st.set_row(&r_);

                    }

    virtual void pre_fetch() {}
    virtual void post_fetch(bool gotData, bool /* calledFromFetch */)
    {
        r_.reset_get_counter();

        if (gotData)
        {
                                                convert_from_base();
        }
    }

    virtual void clean_up() {}

    virtual std::size_t size() const { return 1; }

    virtual void convert_from_base() {}

    row & r_;
};

template <>
struct exchange_traits<row>
{
    typedef basic_type_tag type_family;
};

} // namespace details

} // namespace soci

#endif
