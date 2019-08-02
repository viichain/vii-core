#pragma once


#include <string>

namespace viichain
{

struct SecretValue
{
    std::string value;
};

bool operator==(SecretValue const& x, SecretValue const& y);
bool operator!=(SecretValue const& x, SecretValue const& y);
}
