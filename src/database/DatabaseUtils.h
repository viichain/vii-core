#pragma once


#include "Database.h"

namespace viichain
{
namespace DatabaseUtils
{
void deleteOldEntriesHelper(soci::session& sess, uint32_t ledgerSeq,
                            uint32_t count, std::string const& tableName,
                            std::string const& ledgerSeqColumn);
}
}
