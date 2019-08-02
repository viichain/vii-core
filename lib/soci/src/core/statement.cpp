
#define SOCI_SOURCE
#include "statement.h"
#include "session.h"
#include "into-type.h"
#include "use-type.h"
#include "values.h"
#include <ctime>
#include <cctype>

#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

using namespace soci;
using namespace soci::details;

void statement::exchange(into_type_ptr const & i)
{
    impl_->exchange(i);
}

void statement::exchange(use_type_ptr const & u)
{
    impl_->exchange(u);
}

statement_impl::statement_impl(session & s)
    : session_(s), refCount_(1), row_(0),
      fetchSize_(1), initialFetchSize_(1),
      alreadyDescribed_(false)
{
    backEnd_ = s.make_statement_backend();
}

statement_impl::statement_impl(prepare_temp_type const & prep)
    : session_(prep.get_prepare_info()->session_),
      refCount_(1), row_(0), fetchSize_(1), alreadyDescribed_(false)
{
    backEnd_ = session_.make_statement_backend();

    ref_counted_prepare_info * prepInfo = prep.get_prepare_info();

        intos_.swap(prepInfo->intos_);
    uses_.swap(prepInfo->uses_);

        alloc();

        query_ = prepInfo->get_query();
    try
    {
        prepare(query_);
    }
    catch(...)
    {
        clean_up();
        throw;
    }

    define_and_bind();
}

statement_impl::~statement_impl()
{
    clean_up();
}

void statement_impl::alloc()
{
    backEnd_->alloc();
}

void statement_impl::bind(values & values)
{
    std::size_t cnt = 0;

    try
    {
        for (std::vector<details::standard_use_type*>::iterator it =
            values.uses_.begin(); it != values.uses_.end(); ++it)
        {
                                    
            std::string const& useName = (*it)->get_name();
            if (useName.empty())
            {
                
                int position = static_cast<int>(uses_.size());
                (*it)->bind(*this, position);
                uses_.push_back(*it);
                indicators_.push_back(values.indicators_[cnt]);
            }
            else
            {
                                std::string const placeholder = ":" + useName;

                std::size_t pos = query_.find(placeholder);
                while (pos != std::string::npos)
                {
                                                            const char nextChar = (pos + placeholder.size()) < query_.size() ?
                                          query_[pos + placeholder.size()] : '\0';
                    
                    if (std::isalnum(nextChar))
                    {
                                                                        pos = query_.find(placeholder, pos + placeholder.size());
                    }
                    else
                    {
                        int position = static_cast<int>(uses_.size());
                        (*it)->bind(*this, position);
                        uses_.push_back(*it);
                        indicators_.push_back(values.indicators_[cnt]);
                                                break;
                    }
                }
                                if (pos == std::string::npos)
                {
                    values.add_unused(*it, values.indicators_[cnt]);
                }
            }

            cnt++;
        }
    }
    catch (...)
    {
        for (std::size_t i = ++cnt; i != values.uses_.size(); ++i)
        {
            values.add_unused(values.uses_[i], values.indicators_[i]);
        }
        throw;
    }
}

void statement_impl::exchange(into_type_ptr const & i)
{
    intos_.push_back(i.get());
    i.release();
}

void statement_impl::exchange_for_row(into_type_ptr const & i)
{
    intosForRow_.push_back(i.get());
    i.release();
}

void statement_impl::exchange_for_rowset(into_type_ptr const & i)
{
    if (intos_.empty() == false)
    {
        throw soci_error("Explicit into elements not allowed with rowset.");
    }

    into_type_base* p = i.get();
    intos_.push_back(p);
    i.release();

    int definePosition = 1;
    p->define(*this, definePosition);
    definePositionForRow_ = definePosition;
}

void statement_impl::exchange(use_type_ptr const & u)
{
    uses_.push_back(u.get());
    u.release();
}

void statement_impl::clean_up(bool freeBackend)
{
        std::size_t const isize = intos_.size();
    for (std::size_t i = isize; i != 0; --i)
    {
        intos_[i - 1]->clean_up();
        delete intos_[i - 1];
        intos_.resize(i - 1);
    }

    std::size_t const ifrsize = intosForRow_.size();
    for (std::size_t i = ifrsize; i != 0; --i)
    {
        intosForRow_[i - 1]->clean_up();
        delete intosForRow_[i - 1];
        intosForRow_.resize(i - 1);
    }

    std::size_t const usize = uses_.size();
    for (std::size_t i = usize; i != 0; --i)
    {
        uses_[i - 1]->clean_up();
        delete uses_[i - 1];
        uses_.resize(i - 1);
    }

    std::size_t const indsize = indicators_.size();
    for (std::size_t i = 0; i != indsize; ++i)
    {
        delete indicators_[i];
        indicators_[i] = NULL;
    }

    if (freeBackend && backEnd_ != NULL)
    {
        backEnd_->clean_up();
        delete backEnd_;
        backEnd_ = NULL;
    }
}

void statement_impl::prepare(std::string const & query,
    statement_type eType)
{
    query_ = query;
    session_.log_query(query);

    backEnd_->prepare(query, eType);
}

void statement_impl::define_and_bind()
{
    int definePosition = 1;
    std::size_t const isize = intos_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        intos_[i]->define(*this, definePosition);
    }

                    definePositionForRow_ = definePosition;

    int bindPosition = 1;
    std::size_t const usize = uses_.size();
    for (std::size_t i = 0; i != usize; ++i)
    {
        uses_[i]->bind(*this, bindPosition);
    }
}

void statement_impl::define_for_row()
{
    std::size_t const isize = intosForRow_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        intosForRow_[i]->define(*this, definePositionForRow_);
    }
}

void statement_impl::undefine_and_bind()
{
    std::size_t const isize = intos_.size();
    for (std::size_t i = isize; i != 0; --i)
    {
        intos_[i - 1]->clean_up();
    }

    std::size_t const ifrsize = intosForRow_.size();
    for (std::size_t i = ifrsize; i != 0; --i)
    {
        intosForRow_[i - 1]->clean_up();
    }

    std::size_t const usize = uses_.size();
    for (std::size_t i = usize; i != 0; --i)
    {
        uses_[i - 1]->clean_up();
    }
}

bool statement_impl::execute(bool withDataExchange)
{
    initialFetchSize_ = intos_size();

    if (intos_.empty() == false && initialFetchSize_ == 0)
    {
                        throw soci_error("Vectors of size 0 are not allowed.");
    }

    fetchSize_ = initialFetchSize_;

        
    pre_use();

    std::size_t const bindSize = uses_size();

    if (bindSize > 1 && fetchSize_ > 1)
    {
        throw soci_error(
             "Bulk insert/update and bulk select not allowed in same query");
    }

                        if (row_ != NULL && alreadyDescribed_ == false)
    {
        describe();
        define_for_row();
    }

    int num = 0;
    if (withDataExchange)
    {
        num = 1;

        pre_fetch();

        if (static_cast<int>(fetchSize_) > num)
        {
            num = static_cast<int>(fetchSize_);
        }
        if (static_cast<int>(bindSize) > num)
        {
            num = static_cast<int>(bindSize);
        }
    }

    statement_backend::exec_fetch_result res = backEnd_->execute(num);

    bool gotData = false;

    if (res == statement_backend::ef_success)
    {
                
        if (num > 0)
        {
            gotData = true;

                        resize_intos(static_cast<std::size_t>(num));
        }
    }
    else // res == ef_no_data
    {
                        
        gotData = fetchSize_ > 1 ? resize_intos() : false;
    }

    if (num > 0)
    {
        post_fetch(gotData, false);
    }
    
    post_use(gotData);

    session_.set_got_data(gotData);
    return gotData;
}

long long statement_impl::get_affected_rows()
{
    return backEnd_->get_affected_rows();
}

bool statement_impl::fetch()
{
    if (fetchSize_ == 0)
    {
        truncate_intos();
        session_.set_got_data(false);
        return false;
    }

    bool gotData = false;

        std::size_t const newFetchSize = intos_size();
    if (newFetchSize > initialFetchSize_)
    {
                
        throw soci_error(
            "Increasing the size of the output vector is not supported.");
    }
    else if (newFetchSize == 0)
    {
        session_.set_got_data(false);
        return false;
    }
    else
    {
                fetchSize_ = newFetchSize;
    }

    statement_backend::exec_fetch_result const res = backEnd_->fetch(static_cast<int>(fetchSize_));
    if (res == statement_backend::ef_success)
    {
                
        gotData = true;

                resize_intos(fetchSize_);
    }
    else // res == ef_no_data
    {
        
        if (fetchSize_ > 1)
        {
                        gotData = resize_intos();
            fetchSize_ = 0;
        }
        else
        {
            truncate_intos();
            gotData = false;
        }
    }

    post_fetch(gotData, true);
    session_.set_got_data(gotData);
    return gotData;
}

std::size_t statement_impl::intos_size()
{
            
    std::size_t intos_size = 0;
    std::size_t const isize = intos_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        if (i==0)
        {
            intos_size = intos_[i]->size();
        }
        else if (intos_size != intos_[i]->size())
        {
            std::ostringstream msg;
            msg << "Bind variable size mismatch (into["
                << static_cast<unsigned long>(i) << "] has size "
                << static_cast<unsigned long>(intos_[i]->size())
                << ", into[0] has size "
                << static_cast<unsigned long>(intos_size);
            throw soci_error(msg.str());
        }
    }
    return intos_size;
}

std::size_t statement_impl::uses_size()
{
    std::size_t usesSize = 0;
    std::size_t const usize = uses_.size();
    for (std::size_t i = 0; i != usize; ++i)
    {
        if (i==0)
        {
            usesSize = uses_[i]->size();
            if (usesSize == 0)
            {
                                  throw soci_error("Vectors of size 0 are not allowed.");
            }
        }
        else if (usesSize != uses_[i]->size())
        {
            std::ostringstream msg;
            msg << "Bind variable size mismatch (use["
                << static_cast<unsigned long>(i) << "] has size "
                << static_cast<unsigned long>(uses_[i]->size())
                << ", use[0] has size "
                << static_cast<unsigned long>(usesSize);
            throw soci_error(msg.str());
        }
    }
    return usesSize;
}

bool statement_impl::resize_intos(std::size_t upperBound)
{
        
    int rows = backEnd_->get_number_of_rows();
    if (rows < 0)
    {
        rows = 0;
    }
    if (upperBound != 0 && upperBound < static_cast<std::size_t>(rows))
    {
        rows = static_cast<int>(upperBound);
    }

    std::size_t const isize = intos_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        intos_[i]->resize((std::size_t)rows);
    }

    return rows > 0 ? true : false;
}

void statement_impl::truncate_intos()
{
    std::size_t const isize = intos_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        intos_[i]->resize(0);
    }
}

void statement_impl::pre_fetch()
{
    std::size_t const isize = intos_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        intos_[i]->pre_fetch();
    }

    std::size_t const ifrsize = intosForRow_.size();
    for (std::size_t i = 0; i != ifrsize; ++i)
    {
        intosForRow_[i]->pre_fetch();
    }
}

void statement_impl::pre_use()
{
    std::size_t const usize = uses_.size();
    for (std::size_t i = 0; i != usize; ++i)
    {
        uses_[i]->pre_use();
    }
}

void statement_impl::post_fetch(bool gotData, bool calledFromFetch)
{
            
    std::size_t const ifrsize = intosForRow_.size();
    for (std::size_t i = 0; i != ifrsize; ++i)
    {
        intosForRow_[i]->post_fetch(gotData, calledFromFetch);
    }

    std::size_t const isize = intos_.size();
    for (std::size_t i = 0; i != isize; ++i)
    {
        intos_[i]->post_fetch(gotData, calledFromFetch);
    }
}

void statement_impl::post_use(bool gotData)
{
            for (std::size_t i = uses_.size(); i != 0; --i)
    {
        uses_[i-1]->post_use(gotData);
    }
}

namespace soci
{
namespace details
{


template<>
void statement_impl::bind_into<dt_string>()
{
    into_row<std::string>();
}

template<>
void statement_impl::bind_into<dt_double>()
{
    into_row<double>();
}

template<>
void statement_impl::bind_into<dt_integer>()
{
    into_row<int>();
}

template<>
void statement_impl::bind_into<dt_long_long>()
{
    into_row<long long>();
}

template<>
void statement_impl::bind_into<dt_unsigned_long_long>()
{
    into_row<unsigned long long>();
}

template<>
void statement_impl::bind_into<dt_date>()
{
    into_row<std::tm>();
}

void statement_impl::describe()
{
    row_->clean_up();

    int const numcols = backEnd_->prepare_for_describe();
    for (int i = 1; i <= numcols; ++i)
    {
        data_type dtype;
        std::string columnName;

        backEnd_->describe_column(i, dtype, columnName);

        column_properties props;
        props.set_name(columnName);
        props.set_data_type(dtype);

        switch (dtype)
        {
        case dt_string:
            bind_into<dt_string>();
            break;
        case dt_double:
            bind_into<dt_double>();
            break;
        case dt_integer:
            bind_into<dt_integer>();
            break;
        case dt_long_long:
            bind_into<dt_long_long>();
            break;
        case dt_unsigned_long_long:
            bind_into<dt_unsigned_long_long>();
            break;
        case dt_date:
            bind_into<dt_date>();
            break;
        default:
            std::ostringstream msg;
            msg << "db column type " << dtype
                <<" not supported for dynamic selects"<<std::endl;
            throw soci_error(msg.str());
        }
        row_->add_properties(props);
    }

    alreadyDescribed_ = true;
}

} // namespace details
} // namespace soci

void statement_impl::set_row(row * r)
{
    if (row_ != NULL)
    {
        throw soci_error(
            "Only one Row element allowed in a single statement.");
    }

    row_ = r;
    row_->uppercase_column_names(session_.get_uppercase_column_names());
}

std::string statement_impl::rewrite_for_procedure_call(std::string const & query)
{
    return backEnd_->rewrite_for_procedure_call(query);
}

void statement_impl::inc_ref()
{
    ++refCount_;
}

void statement_impl::dec_ref()
{
    if (--refCount_ == 0)
    {
        delete this;
    }
}

standard_into_type_backend *
statement_impl::make_into_type_backend()
{
    return backEnd_->make_into_type_backend();
}

standard_use_type_backend *
statement_impl::make_use_type_backend()
{
    return backEnd_->make_use_type_backend();
}

vector_into_type_backend *
statement_impl::make_vector_into_type_backend()
{
    return backEnd_->make_vector_into_type_backend();
}

vector_use_type_backend *
statement_impl::make_vector_use_type_backend()
{
    return backEnd_->make_vector_use_type_backend();
}
