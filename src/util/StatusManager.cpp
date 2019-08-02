
#include "StatusManager.h"

#include <array>

namespace viichain
{

StatusManager::StatusManager()
{
}

StatusManager::~StatusManager()
{
}

void
StatusManager::setStatusMessage(StatusCategory issue, std::string message)
{
    mStatusMessages[issue] = std::move(message);
}

void
StatusManager::removeStatusMessage(StatusCategory issue)
{
    mStatusMessages.erase(issue);
}

std::string
StatusManager::getStatusMessage(StatusCategory issue) const
{
    auto it = mStatusMessages.find(issue);
    if (it == mStatusMessages.end())
    {
        return std::string{};
    }
    else
    {
        return it->second;
    }
}
}
