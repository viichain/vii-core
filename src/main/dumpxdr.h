#pragma once


#include "overlay/VIIXDR.h"

namespace viichain
{

void dumpXdrStream(std::string const& filename);
void printXdr(std::string const& filename, std::string const& filetype,
              bool base64);
void signtxn(std::string const& filename, std::string netId, bool base64);
void priv2pub();
}
