
#ifndef SOCI_SESSION_H_INCLUDED
#define SOCI_SESSION_H_INCLUDED

#include "once-temp-type.h"
#include "query_transformation.h"
#include "connection-parameters.h"

#include <cstddef>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace soci
{
class values;
class backend_factory;

namespace details
{

class session_backend;
class statement_backend;
class rowid_backend;
class blob_backend;

} // namespace details

class connection_pool;

class SOCI_DECL session
{
private:

    void set_query_transformation_(std::auto_ptr<details::query_transformation_function> qtf);

public:
    session();
    explicit session(connection_parameters const & parameters);
    session(backend_factory const & factory, std::string const & connectString);
    session(std::string const & backendName, std::string const & connectString);
    explicit session(std::string const & connectString);
    explicit session(connection_pool & pool);

    ~session();

    void open(connection_parameters const & parameters);
    void open(backend_factory const & factory, std::string const & connectString);
    void open(std::string const & backendName, std::string const & connectString);
    void open(std::string const & connectString);
    void close();
    void reconnect();

    void begin();
    void commit();
    void rollback();

        details::once_type once;
    details::prepare_type prepare;

        template <typename T>
    details::once_temp_type operator<<(T const & t) { return once << t; }

    std::ostringstream & get_query_stream();
    std::string get_query() const;

    template <typename T>
    void set_query_transformation(T callback)
    {
        std::auto_ptr<details::query_transformation_function> qtf(new details::query_transformation<T>(callback));
        set_query_transformation_(qtf);

        assert(qtf.get() == NULL);
    }

        void set_log_stream(std::ostream * s);
    std::ostream * get_log_stream() const;

    void log_query(std::string const & query);
    std::string get_last_query() const;

    void set_got_data(bool gotData);
    bool got_data() const;

    void uppercase_column_names(bool forceToUpper);

    bool get_uppercase_column_names() const;

    
                        bool get_next_sequence_value(std::string const & sequence, long & value);

                bool get_last_insert_id(std::string const & table, long & value);


            details::session_backend * get_backend() { return backEnd_; }

    std::string get_backend_name() const;

    details::statement_backend * make_statement_backend();
    details::rowid_backend * make_rowid_backend();
    details::blob_backend * make_blob_backend();

private:
    session(session const &);
    session& operator=(session const &);

    std::ostringstream query_stream_;
    details::query_transformation_function* query_transformation_;

    std::ostream * logStream_;
    std::string lastQuery_;

    connection_parameters lastConnectParameters_;

    bool uppercaseColumnNames_;

    details::session_backend * backEnd_;

    bool gotData_;

    bool isFromPool_;
    std::size_t poolPosition_;
    connection_pool * pool_;

    void transaction_commit_helper(bool commit, std::string &sql);
    int transaction_level_;
};

} // namespace soci

#endif // SOCI_SESSION_H_INCLUDED
