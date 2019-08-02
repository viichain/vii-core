#pragma once


#include <string>

namespace viichain
{

class Config;

void checkQuorumIntersection(Config const& cfg, uint32_t ledgerNum);
void inferQuorumAndWrite(Config const& cfg, uint32_t ledgerNum);
void writeQuorumGraph(Config const& cfg, std::string const& outputFile,
                      uint32_t ledgerNum);
}
