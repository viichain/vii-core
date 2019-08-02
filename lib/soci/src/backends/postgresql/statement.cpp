
#define SOCI_POSTGRESQL_SOURCE
#include "soci-postgresql.h"
#include <soci-platform.h>
#include <libpq/libpq-fs.h> // libpq
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>

#ifdef SOCI_POSTGRESQL_NOPARAMS
#ifndef SOCI_POSTGRESQL_NOBINDBYNAME
#define SOCI_POSTGRESQL_NOBINDBYNAME
#endif // SOCI_POSTGRESQL_NOBINDBYNAME
#endif // SOCI_POSTGRESQL_NOPARAMS

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

using namespace soci;
using namespace soci::details;

postgresql_statement_backend::postgresql_statement_backend(
    postgresql_session_backend &session)
     : session_(session)
     , rowsAffectedBulk_(-1LL), justDescribed_(false)
     , hasIntoElements_(false), hasVectorIntoElements_(false)
     , hasUseElements_(false), hasVectorUseElements_(false)
{
}

postgresql_statement_backend::~postgresql_statement_backend()
{
    if (statementName_.empty() == false)
    {
        try
        {
            session_.deallocate_prepared_statement(statementName_);
        }
        catch (...)
        {
                                                        }
    }
}

void postgresql_statement_backend::alloc()
{
    }

void postgresql_statement_backend::clean_up()
{
            rowsAffectedBulk_ = -1;
    
    }

void postgresql_statement_backend::prepare(std::string const & query,
    statement_type stType)
{
#ifdef SOCI_POSTGRESQL_NOBINDBYNAME
    query_ = query;
#else
        
    enum { normal, in_quotes, in_name } state = normal;

    std::string name;
    int position = 1;

    for (std::string::const_iterator it = query.begin(), end = query.end();
         it != end; ++it)
    {
        switch (state)
        {
        case normal:
            if (*it == '\'')
            {
                query_ += *it;
                state = in_quotes;
            }
            else if (*it == ':')
            {
                                                const std::string::const_iterator next_it = it + 1;
                if ((next_it != end) && (*next_it == ':'))
                {
                    query_ += "::";
                    ++it;
                }
                                                else if ((next_it != end) && (*next_it == '='))
                {
                    query_ += ":=";
                    ++it;
                }
                else
                {
                    state = in_name;
                }
            }
            else // regular character, stay in the same state
            {
                query_ += *it;
            }
            break;
        case in_quotes:
            if (*it == '\'')
            {
                query_ += *it;
                state = normal;
            }
            else // regular quoted character
            {
                query_ += *it;
            }
            break;
        case in_name:
            if (std::isalnum(*it) || *it == '_')
            {
                name += *it;
            }
            else // end of name
            {
                names_.push_back(name);
                name.clear();
                std::ostringstream ss;
                ss << '$' << position++;
                query_ += ss.str();
                query_ += *it;
                state = normal;

                                                                                if (*it == ':')
                {
                    const std::string::const_iterator next_it = it + 1;
                    if ((next_it != end) && (*next_it == ':'))
                    {
                        query_ += ':';
                        ++it;
                    }
                }
            }
            break;
        }
    }

    if (state == in_name)
    {
        names_.push_back(name);
        std::ostringstream ss;
        ss << '$' << position++;
        query_ += ss.str();
    }

#endif // SOCI_POSTGRESQL_NOBINDBYNAME

#ifndef SOCI_POSTGRESQL_NOPREPARE

    if (stType == st_repeatable_query)
    {
        assert(statementName_.empty());

                        std::string statementName = session_.get_next_statement_name();

        postgresql_result result(
            PQprepare(session_.conn_, statementName.c_str(),
              query_.c_str(), static_cast<int>(names_.size()), NULL));
        result.check_for_errors("Cannot prepare statement.");

                statementName_ = statementName;
    }

    stType_ = stType;

#endif // SOCI_POSTGRESQL_NOPREPARE
}

statement_backend::exec_fetch_result
postgresql_statement_backend::execute(int number)
{
                    
    if (justDescribed_ == false)
    {
                clean_up();

        if (number > 1 && hasIntoElements_)
        {
             throw soci_error(
                  "Bulk use with single into elements is not supported.");
        }

                                                                
        int numberOfExecutions = 1;
        if (number > 0)
        {
             numberOfExecutions = hasUseElements_ ? 1 : number;
        }

        if ((useByPosBuffers_.empty() == false) ||
            (useByNameBuffers_.empty() == false))
        {
            if ((useByPosBuffers_.empty() == false) &&
                (useByNameBuffers_.empty() == false))
            {
                throw soci_error(
                    "Binding for use elements must be either by position "
                    "or by name.");
            }
            long long rowsAffectedBulkTemp = 0;
            for (int i = 0; i != numberOfExecutions; ++i)
            {
                std::vector<char *> paramValues;

                if (useByPosBuffers_.empty() == false)
                {
                                                            
                    for (UseByPosBuffersMap::iterator
                             it = useByPosBuffers_.begin(),
                             end = useByPosBuffers_.end();
                         it != end; ++it)
                    {
                        char ** buffers = it->second;
                        paramValues.push_back(buffers[i]);
                    }
                }
                else
                {
                    
                    for (std::vector<std::string>::iterator
                             it = names_.begin(), end = names_.end();
                         it != end; ++it)
                    {
                        UseByNameBuffersMap::iterator b
                            = useByNameBuffers_.find(*it);
                        if (b == useByNameBuffers_.end())
                        {
                            std::string msg(
                                "Missing use element for bind by name (");
                            msg += *it;
                            msg += ").";
                            throw soci_error(msg);
                        }
                        char ** buffers = b->second;
                        paramValues.push_back(buffers[i]);
                    }
                }

#ifdef SOCI_POSTGRESQL_NOPARAMS

                throw soci_error("Queries with parameters are not supported.");

#else

#ifdef SOCI_POSTGRESQL_NOPREPARE

                result_.reset(PQexecParams(session_.conn_, query_.c_str(),
                    static_cast<int>(paramValues.size()),
                    NULL, &paramValues[0], NULL, NULL, 0));
#else
                if (stType_ == st_repeatable_query)
                {
                    
                    result_.reset(PQexecPrepared(session_.conn_,
                        statementName_.c_str(),
                        static_cast<int>(paramValues.size()),
                        &paramValues[0], NULL, NULL, 0));
                }
                else // stType_ == st_one_time_query
                {
                                        
                    result_.reset(PQexecParams(session_.conn_, query_.c_str(),
                        static_cast<int>(paramValues.size()),
                        NULL, &paramValues[0], NULL, NULL, 0));
                }

#endif // SOCI_POSTGRESQL_NOPREPARE

#endif // SOCI_POSTGRESQL_NOPARAMS

                if (numberOfExecutions > 1)
                {
                    
                                        rowsAffectedBulk_ = rowsAffectedBulkTemp;
                    
                    result_.check_for_errors("Cannot execute query.");
                    
                    rowsAffectedBulkTemp += get_affected_rows();                    
                }
            }
            rowsAffectedBulk_ = rowsAffectedBulkTemp;

            if (numberOfExecutions > 1)
            {
                                result_.reset();
                return ef_no_data;
            }

                    }
        else
        {
                        
#ifdef SOCI_POSTGRESQL_NOPREPARE

            result_.reset(PQexec(session_.conn_, query_.c_str()));
#else
            if (stType_ == st_repeatable_query)
            {
                
                result_.reset(PQexecPrepared(session_.conn_,
                    statementName_.c_str(), 0, NULL, NULL, NULL, 0));
            }
            else // stType_ == st_one_time_query
            {
                result_.reset(PQexec(session_.conn_, query_.c_str()));
            }

#endif // SOCI_POSTGRESQL_NOPREPARE
        }
    }
    else
    {
                                
        justDescribed_ = false;
    }

    if (result_.check_for_data("Cannot execute query."))
    {
        currentRow_ = 0;
        rowsToConsume_ = 0;

        numberOfRows_ = PQntuples(result_);
        if (numberOfRows_ == 0)
        {
            return ef_no_data;
        }
        else
        {
            if (number > 0)
            {
                                return fetch(number);
            }
            else
            {
                                return ef_success;
            }
        }
    }
    else
    {
      return ef_no_data;
    }
}

statement_backend::exec_fetch_result
postgresql_statement_backend::fetch(int number)
{
                    
        currentRow_ += rowsToConsume_;

    if (currentRow_ >= numberOfRows_)
    {
                return ef_no_data;
    }
    else
    {
        if (currentRow_ + number > numberOfRows_)
        {
            rowsToConsume_ = numberOfRows_ - currentRow_;

                                                return ef_no_data;
        }
        else
        {
            rowsToConsume_ = number;
            return ef_success;
        }
    }
}

long long postgresql_statement_backend::get_affected_rows()
{
            const char * const resultStr = PQcmdTuples(result_.get_result());
    char * end;
    long long result = std::strtoll(resultStr, &end, 0);
    if (end != resultStr)
    {
        return result;
    }
    else if (rowsAffectedBulk_ >= 0)
    {
        return rowsAffectedBulk_;
    }
    else
    {
        return -1;
    }
}

int postgresql_statement_backend::get_number_of_rows()
{
    return numberOfRows_ - currentRow_;
}

std::string postgresql_statement_backend::rewrite_for_procedure_call(
    std::string const & query)
{
    std::string newQuery("select ");
    newQuery += query;
    return newQuery;
}

int postgresql_statement_backend::prepare_for_describe()
{
    execute(1);
    justDescribed_ = true;

    int columns = PQnfields(result_);
    return columns;
}

void postgresql_statement_backend::describe_column(int colNum, data_type & type,
    std::string & columnName)
{
        int const pos = colNum - 1;

    unsigned long const typeOid = PQftype(result_, pos);
    switch (typeOid)
    {
        
               
    case 25:   // text
    case 1043: // varchar
    case 2275: // cstring
    case 18:   // char
    case 1042: // bpchar
    case 142: // xml
    case 114:  // json
    case 17: // bytea
        type = dt_string;
        break;

    case 702:  // abstime
    case 703:  // reltime
    case 1082: // date
    case 1083: // time
    case 1114: // timestamp
    case 1184: // timestamptz
    case 1266: // timetz
        type = dt_date;
        break;

    case 700:  // float4
    case 701:  // float8
    case 1700: // numeric
        type = dt_double;
        break;

    case 16:   // bool
    case 21:   // int2
    case 23:   // int4
    case 26:   // oid
        type = dt_integer;
        break;

    case 20:   // int8
        type = dt_long_long;
        break;
    
    default:
    {
        int form = PQfformat(result_, pos);
        int size = PQfsize(result_, pos);
        if (form == 0 && size == -1)
        {
            type = dt_string;
        }
        else
        {
            std::stringstream message;
            message << "unknown data type with typelem: " << typeOid << " for colNum: " << colNum << " with name: " << PQfname(result_, pos);
            throw soci_error(message.str());
        }
    }
    }

    columnName = PQfname(result_, pos);
}

postgresql_standard_into_type_backend *
postgresql_statement_backend::make_into_type_backend()
{
    hasIntoElements_ = true;
    return new postgresql_standard_into_type_backend(*this);
}

postgresql_standard_use_type_backend *
postgresql_statement_backend::make_use_type_backend()
{
    hasUseElements_ = true;
    return new postgresql_standard_use_type_backend(*this);
}

postgresql_vector_into_type_backend *
postgresql_statement_backend::make_vector_into_type_backend()
{
    hasVectorIntoElements_ = true;
    return new postgresql_vector_into_type_backend(*this);
}

postgresql_vector_use_type_backend *
postgresql_statement_backend::make_vector_use_type_backend()
{
    hasVectorUseElements_ = true;
    return new postgresql_vector_use_type_backend(*this);
}
