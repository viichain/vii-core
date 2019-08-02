
#define SOCI_SOURCE

#include "error.h"

using namespace soci;

soci_error::soci_error(std::string const & msg)
     : std::runtime_error(msg)
{
}
