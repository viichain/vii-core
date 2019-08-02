#pragma once


#include <stdexcept>

namespace viichain
{

class InvariantDoesNotHold : public std::runtime_error
{
  public:
    explicit InvariantDoesNotHold(std::string const& msg)
        : std::runtime_error{msg}
    {
    }
    virtual ~InvariantDoesNotHold() = default;
};
}
