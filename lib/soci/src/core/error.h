
#ifndef SOCI_ERROR_H_INCLUDED
#define SOCI_ERROR_H_INCLUDED

#include "soci-config.h"
#include <stdexcept>
#include <string>

namespace soci
{

class SOCI_DECL soci_error : public std::runtime_error
{
public:
    explicit soci_error(std::string const & msg);
};

} // namespace soci

#endif // SOCI_ERROR_H_INCLUDED
