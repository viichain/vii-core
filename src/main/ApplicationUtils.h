#pragma once


#include "main/Application.h"

namespace viichain
{

class CatchupConfiguration;

int runWithConfig(Config cfg);
void setForceSCPFlag(Config cfg, bool set);
void initializeDatabase(Config cfg);
void httpCommand(std::string const& command, unsigned short port);
void loadXdr(Config cfg, std::string const& bucketFile);
void showOfflineInfo(Config cfg);
int reportLastHistoryCheckpoint(Config cfg, std::string const& outputFile);
void genSeed();
int initializeHistories(Config cfg,
                        std::vector<std::string> const& newHistories);
void writeCatchupInfo(Json::Value const& catchupInfo,
                      std::string const& outputFile);
int catchup(Application::pointer app, CatchupConfiguration cc,
            Json::Value& catchupInfo);
int publish(Application::pointer app);
}
