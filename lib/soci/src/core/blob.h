
#ifndef SOCI_BLOB_H_INCLUDED
#define SOCI_BLOB_H_INCLUDED

#include "soci-config.h"
#include <cstddef>

namespace soci
{

class session;

namespace details
{
class blob_backend;
} // namespace details

class SOCI_DECL blob
{
public:
    explicit blob(session & s);
    ~blob();

    std::size_t get_len();
    std::size_t read(std::size_t offset, char * buf, std::size_t toRead);
    std::size_t write(std::size_t offset, char const * buf,
        std::size_t toWrite);
    std::size_t append(char const * buf, std::size_t toWrite);
    void trim(std::size_t newLen);

    details::blob_backend * get_backend() { return backEnd_; }

private:
    details::blob_backend * backEnd_;
};

} // namespace soci

#endif
