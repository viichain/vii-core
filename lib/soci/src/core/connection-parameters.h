
#ifndef SOCI_CONNECTION_PARAMETERS_H_INCLUDED
#define SOCI_CONNECTION_PARAMETERS_H_INCLUDED

#include "soci-config.h"

#include <map>
#include <string>

namespace soci
{

class backend_factory;

class SOCI_DECL connection_parameters
{
public:
    connection_parameters();
    connection_parameters(backend_factory const & factory, std::string const & connectString);
    connection_parameters(std::string const & backendName, std::string const & connectString);
    explicit connection_parameters(std::string const & fullConnectString);

    

        backend_factory const * get_factory() const { return factory_; }
    std::string const & get_connect_string() const { return connectString_; }

        void set_option(const char * name, std::string const & value)
    {
        options_[name] = value;
    }

            bool get_option(const char * name, std::string & value) const
    {
        Options::const_iterator const it = options_.find(name);
        if (it == options_.end())
            return false;

        value = it->second;

        return true;
    }

private:
        backend_factory const * factory_;
    std::string connectString_;

        typedef std::map<const char*, std::string> Options;
    Options options_;
};

} // namespace soci

#endif // SOCI_CONNECTION_PARAMETERS_H_INCLUDED
