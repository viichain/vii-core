
#include "herder/Herder.h"

namespace viichain
{

std::chrono::seconds const Herder::EXP_LEDGER_TIMESPAN_SECONDS(5);
std::chrono::seconds const Herder::MAX_SCP_TIMEOUT_SECONDS(240);
std::chrono::seconds const Herder::CONSENSUS_STUCK_TIMEOUT_SECONDS(35);
std::chrono::seconds const Herder::MAX_TIME_SLIP_SECONDS(60);
std::chrono::seconds const Herder::NODE_EXPIRATION_SECONDS(240);
uint32 const Herder::LEDGER_VALIDITY_BRACKET = 100;
uint32 const Herder::MAX_SLOTS_TO_REMEMBER = 12;
std::chrono::nanoseconds const Herder::TIMERS_THRESHOLD_NANOSEC(5000000);
}
