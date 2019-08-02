#pragma once


#include "util/Logging.h"

namespace viichain
{

void fuzz(std::string const& filename, el::Level logLevel,
          std::vector<std::string> const& metrics);
void genfuzz(std::string const& filename);
}
