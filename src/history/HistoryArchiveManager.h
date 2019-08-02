#pragma once


#include <memory>
#include <string>
#include <vector>

namespace viichain
{
class Application;
class Config;
class HistoryArchive;

class HistoryArchiveManager
{
  public:
    explicit HistoryArchiveManager(Application& app);

        bool checkSensibleConfig() const;

            std::shared_ptr<HistoryArchive> selectRandomReadableHistoryArchive() const;

            bool initializeHistoryArchive(std::string const& arch) const;

            bool hasAnyWritableHistoryArchive() const;

        std::shared_ptr<HistoryArchive>
    getHistoryArchive(std::string const& name) const;

            std::vector<std::shared_ptr<HistoryArchive>>
    getWritableHistoryArchives() const;

    double getFailureRate() const;

  private:
    Application& mApp;
    std::vector<std::shared_ptr<HistoryArchive>> mArchives;
};
}
