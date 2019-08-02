
#include "DatabaseUtils.h"

namespace viichain
{
namespace DatabaseUtils
{
void
deleteOldEntriesHelper(soci::session& sess, uint32_t ledgerSeq, uint32_t count,
                       std::string const& tableName,
                       std::string const& ledgerSeqColumn)
{
    uint32_t curMin = 0;
    soci::indicator gotMin;
    soci::statement st = (sess.prepare << "SELECT MIN(" << ledgerSeqColumn
                                       << ") FROM " << tableName,
                          soci::into(curMin, gotMin));

    st.execute(true);
    if (st.got_data() && gotMin == soci::i_ok)
    {
        uint64 m64 =
            std::min<uint64>(static_cast<uint64>(curMin) + count, ledgerSeq);
                uint64 m = static_cast<uint32>(m64);
        sess << "DELETE FROM " << tableName << " WHERE " << ledgerSeqColumn
             << " <= " << m;
    }
}
}
}