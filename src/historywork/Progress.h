
#include <string>

namespace viichain
{

class Application;

std::string fmtProgress(Application& app, std::string const& task,
                        uint32_t first, uint32_t last, uint32_t curr);
}
