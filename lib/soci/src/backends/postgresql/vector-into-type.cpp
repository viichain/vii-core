
#define SOCI_POSTGRESQL_SOURCE
#include <soci-platform.h>
#include "soci-postgresql.h"
#include "common.h"
#include <libpq/libpq-fs.h> // libpq
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

#ifdef SOCI_POSTGRESQL_NOPARAMS
#ifndef SOCI_POSTGRESQL_NOBINDBYNAME
#define SOCI_POSTGRESQL_NOBINDBYNAME
#endif // SOCI_POSTGRESQL_NOBINDBYNAME
#endif // SOCI_POSTGRESQL_NOPARAMS

using namespace soci;
using namespace soci::details;
using namespace soci::details::postgresql;


void postgresql_vector_into_type_backend::define_by_pos(
    int & position, void * data, exchange_type type)
{
    data_ = data;
    type_ = type;
    position_ = position++;
}

void postgresql_vector_into_type_backend::pre_fetch()
{
    }

namespace // anonymous
{

template <typename T>
void set_invector_(void * p, int indx, T const & val)
{
    std::vector<T> * dest =
        static_cast<std::vector<T> *>(p);

    std::vector<T> & v = *dest;
    v[indx] = val;
}

} // namespace anonymous

void postgresql_vector_into_type_backend::post_fetch(bool gotData, indicator * ind)
{
    if (gotData)
    {
                
                int const pos = position_ - 1;

        int const endRow = statement_.currentRow_ + statement_.rowsToConsume_;

        for (int curRow = statement_.currentRow_, i = 0;
             curRow != endRow; ++curRow, ++i)
        {
                        if (PQgetisnull(statement_.result_, curRow, pos) != 0)
            {
                if (ind == NULL)
                {
                    throw soci_error(
                        "Null value fetched and no indicator defined.");
                }

                ind[i] = i_null;

                                continue;
            }
            else
            {
                if (ind != NULL)
                {
                    ind[i] = i_ok;
                }
            }

                        char * buf = PQgetvalue(statement_.result_, curRow, pos);

            switch (type_)
            {
            case x_char:
                set_invector_(data_, i, *buf);
                break;
            case x_stdstring:
                set_invector_<std::string>(data_, i, buf);
                break;
            case x_short:
                {
                    short const val = string_to_integer<short>(buf);
                    set_invector_(data_, i, val);
                }
                break;
            case x_integer:
                {
                    int const val = string_to_integer<int>(buf);
                    set_invector_(data_, i, val);
                }
                break;
            case x_long_long:
                {
                    long long const val = string_to_integer<long long>(buf);
                    set_invector_(data_, i, val);
                }
                break;
            case x_unsigned_long_long:
                {
                    unsigned long long const val =
                        string_to_unsigned_integer<unsigned long long>(buf);
                    set_invector_(data_, i, val);
                }
                break;
            case x_double:
                {
                    double const val = string_to_double(buf);
                    set_invector_(data_, i, val);
                }
                break;
            case x_stdtm:
                {
                                        std::tm t;
                    parse_std_tm(buf, t);

                    set_invector_(data_, i, t);
                }
                break;

            default:
                throw soci_error("Into element used with non-supported type.");
            }
        }
    }
    else // no data retrieved
    {
            }
}

namespace // anonymous
{

template <typename T>
void resizevector_(void * p, std::size_t sz)
{
    std::vector<T> * v = static_cast<std::vector<T> *>(p);
    v->resize(sz);
}

} // namespace anonymous

void postgresql_vector_into_type_backend::resize(std::size_t sz)
{
    assert(sz < 10u*std::numeric_limits<unsigned short>::max()); // Not a strong constraint, for debugging only. Notice my fix is even worse

    switch (type_)
    {
            case x_char:
        resizevector_<char>(data_, sz);
        break;
    case x_short:
        resizevector_<short>(data_, sz);
        break;
    case x_integer:
        resizevector_<int>(data_, sz);
        break;
    case x_long_long:
        resizevector_<long long>(data_, sz);
        break;
    case x_unsigned_long_long:
        resizevector_<unsigned long long>(data_, sz);
        break;
    case x_double:
        resizevector_<double>(data_, sz);
        break;
    case x_stdstring:
        resizevector_<std::string>(data_, sz);
        break;
    case x_stdtm:
        resizevector_<std::tm>(data_, sz);
        break;
    default:
        throw soci_error("Into vector element used with non-supported type.");
    }
}

std::size_t postgresql_vector_into_type_backend::size()
{
    std::size_t sz = 0; // dummy initialization to please the compiler
    switch (type_)
    {
            case x_char:
        sz = get_vector_size<char>(data_);
        break;
    case x_short:
        sz = get_vector_size<short>(data_);
        break;
    case x_integer:
        sz = get_vector_size<int>(data_);
        break;
    case x_long_long:
        sz = get_vector_size<long long>(data_);
        break;
    case x_unsigned_long_long:
        sz = get_vector_size<unsigned long long>(data_);
        break;
    case x_double:
        sz = get_vector_size<double>(data_);
        break;
    case x_stdstring:
        sz = get_vector_size<std::string>(data_);
        break;
    case x_stdtm:
        sz = get_vector_size<std::tm>(data_);
        break;
    default:
        throw soci_error("Into vector element used with non-supported type.");
    }

    return sz;
}

void postgresql_vector_into_type_backend::clean_up()
{
    }
