
#ifndef SOCI_VALUES_EXCHANGE_H_INCLUDED
#define SOCI_VALUES_EXCHANGE_H_INCLUDED

#include "values.h"
#include "into-type.h"
#include "use-type.h"
#include "row-exchange.h"
#include <cstddef>
#include <string>
#include <vector>

namespace soci
{

namespace details
{

template <>
struct exchange_traits<values>
{
    typedef basic_type_tag type_family;

        enum { x_type = 0 };
};

template <>
class use_type<values> : public use_type_base
{
public:
    use_type(values & v, std::string const & /*name*/ = std::string())
        : v_(v)
    {}

        use_type(values & v, indicator /*ind*/, std::string const & /*name*/ = std::string())
        : v_(v)
    {}

    virtual void bind(details::statement_impl & st, int & /*position*/)
    {
        v_.uppercase_column_names(st.session_.get_uppercase_column_names());

        convert_to_base();
        st.bind(v_);
    }

    virtual void post_use(bool /*gotData*/)
    {
        v_.reset_get_counter();
        convert_from_base();
    }

    virtual void pre_use() {convert_to_base();}
    virtual void clean_up() {v_.clean_up();}
    virtual std::size_t size() const { return 1; }

                virtual void convert_to_base() {}
    virtual void convert_from_base() {}

private:
    values & v_;
};

template <>
class use_type<std::vector<values> >
{
private:
    use_type();
};

template <>
class into_type<values> : public into_type<row>
{
public:
    into_type(values & v)
        : into_type<row>(v.get_row()), v_(v)
    {}
    
    into_type(values & v, indicator & ind)
        : into_type<row>(v.get_row(), ind), v_(v)
    {}

    void clean_up()
    {
        v_.clean_up();
    }

private:
    values & v_;
};

template <>
class into_type<std::vector<values> >
{
private:
    into_type();
};

} // namespace details

} // namespace soci

#endif // SOCI_VALUES_EXCHANGE_H_INCLUDED
