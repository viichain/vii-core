#pragma once


#include <stdexcept>

namespace viichain
{

class FileSystemException : public std::runtime_error
{
  public:
    explicit FileSystemException(std::string const& msg)
        : std::runtime_error{msg}
    {
    }
    virtual ~FileSystemException() = default;
};
}
