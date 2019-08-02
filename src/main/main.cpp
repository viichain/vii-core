
#include "main/CommandLine.h"
#include "util/Logging.h"

#include "crypto/ByteSliceHasher.h"
#include <cstdlib>
#include <sodium/core.h>
#include <xdrpp/marshal.h>

INITIALIZE_EASYLOGGINGPP

namespace viichain
{
static void
outOfMemory()
{
    std::fprintf(stderr, "Unable to allocate memory\n");
    std::fflush(stderr);
    std::abort();
}
}

int
main(int argc, char* const* argv)
{
    using namespace viichain;

        std::set_new_handler(outOfMemory);

    Logging::init();
    if (sodium_init() != 0)
    {
        LOG(FATAL) << "Could not initialize crypto";
        return 1;
    }
    shortHash::initialize();

    xdr::marshaling_stack_limit = 1000;

    return handleCommandLine(argc, argv);
}
