#pragma once


#include "util/Logging.h"
#include "util/optional.h"

#include <string>
#include <vector>

namespace viichain
{

struct CommandLineArgs
{
    std::string mExeName;
    std::string mCommandName;
    std::string mCommandDescription;
    std::vector<std::string> mArgs;
};

int handleCommandLine(int argc, char* const* argv);

void writeWithTextFlow(std::ostream& os, std::string const& text);
}
