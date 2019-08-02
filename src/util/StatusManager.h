#pragma once


#include <map>
#include <string>

namespace viichain
{

enum class StatusCategory
{
    HISTORY_CATCHUP,
    HISTORY_PUBLISH,
    NTP,
    REQUIRES_UPGRADES,
    COUNT
};

class StatusManager
{
  public:
    using storage = std::map<StatusCategory, std::string>;
    using const_iterator = storage::const_iterator;

    explicit StatusManager();
    ~StatusManager();

    void setStatusMessage(StatusCategory issue, std::string message);
    void removeStatusMessage(StatusCategory issue);
    std::string getStatusMessage(StatusCategory issue) const;

    const_iterator
    begin() const
    {
        return mStatusMessages.begin();
    }
    const_iterator
    end() const
    {
        return mStatusMessages.end();
    }
    std::size_t
    size() const
    {
        return mStatusMessages.size();
    }

  private:
    storage mStatusMessages;
};
}
