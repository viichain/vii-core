
#ifndef SOCI_BACKEND_LOADER_H_INCLUDED
#define SOCI_BACKEND_LOADER_H_INCLUDED

#include "soci-backend.h"
#include <string>
#include <vector>

namespace soci
{

namespace dynamic_backends
{

backend_factory const & get(std::string const & name);

SOCI_DECL std::vector<std::string> & search_paths();
SOCI_DECL void register_backend(std::string const & name, std::string const & shared_object = std::string());
SOCI_DECL void register_backend(std::string const & name, backend_factory const & factory);
SOCI_DECL std::vector<std::string> list_all();
SOCI_DECL void unload(std::string const & name);
SOCI_DECL void unload_all();

} // namespace dynamic_backends

} // namespace soci

#endif // SOCI_BACKEND_LOADER_H_INCLUDED
