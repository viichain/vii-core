#pragma once


#include "main/Application.h"
#include "xdr/vii-types.h"
#include <string>

namespace viichain
{

class ExternalQueue
{
  public:
    ExternalQueue(Application& app);

    static void dropAll(Database& db);

        static bool validateResourceID(std::string const& resid);

        void setInitialCursors(std::vector<std::string> const& initialResids);
        void addCursorForResource(std::string const& resid, uint32 cursor);
        void setCursorForResource(std::string const& resid, uint32 cursor);
        void getCursorForResource(std::string const& resid,
                              std::map<std::string, uint32>& curMap);
        void deleteCursor(std::string const& resid);

        void deleteOldEntries(uint32 count);

  private:
    void checkID(std::string const& resid);
    std::string getCursor(std::string const& resid);

    static std::string kSQLCreateStatement;

    Application& mApp;
};
}
