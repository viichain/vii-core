
#include "soci-sqlite3.h"
#include <cstring>
#include <algorithm>

using namespace soci;

sqlite3_blob_backend::sqlite3_blob_backend(sqlite3_session_backend &session)
    : session_(session), buf_(0), len_(0)
{
}

sqlite3_blob_backend::~sqlite3_blob_backend()
{
    if (buf_)
    {
        delete [] buf_;
        buf_ = 0;
        len_ = 0;
    }
}

std::size_t sqlite3_blob_backend::get_len()
{
    return len_;
}

std::size_t sqlite3_blob_backend::read(
    std::size_t offset, char * buf, std::size_t toRead)
{
    size_t r = toRead;

            if (r > len_ - offset)
    {
        r = len_ - offset;
    }

    memcpy(buf, buf_ + offset, r);

    return r;
}


std::size_t sqlite3_blob_backend::write(
    std::size_t offset, char const * buf,
    std::size_t toWrite)
{
    const char* oldBuf = buf_;
    std::size_t oldLen = len_;
    len_ = (std::max)(len_, offset + toWrite);

    buf_ = new char[len_];

    if (oldBuf)
    {
                                memcpy(buf_, oldBuf, oldLen);
        delete [] oldBuf;
    }
    memcpy(buf_ + offset, buf, toWrite);

    return len_;
}


std::size_t sqlite3_blob_backend::append(
    char const * buf, std::size_t toWrite)
{
    const char* oldBuf = buf_;

    buf_ = new char[len_ + toWrite];

    memcpy(buf_, oldBuf, len_);

    memcpy(buf_ + len_, buf, toWrite);

    delete [] oldBuf;

    len_ += toWrite;

    return len_;
}


void sqlite3_blob_backend::trim(std::size_t newLen)
{
    const char* oldBuf = buf_;
    len_ = newLen;

    buf_ = new char[len_];

    memcpy(buf_, oldBuf, len_);

    delete [] oldBuf;
}

std::size_t sqlite3_blob_backend::set_data(char const *buf, std::size_t toWrite)
{
    if (buf_)
    {
        delete [] buf_;
        buf_ = 0;
        len_ = 0;
    }
    return write(0, buf, toWrite);
}
