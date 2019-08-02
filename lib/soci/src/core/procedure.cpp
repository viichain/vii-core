
#define SOCI_SOURCE
#include "procedure.h"
#include "statement.h"
#include "prepare-temp-type.h"

using namespace soci;
using namespace soci::details;

procedure_impl::procedure_impl(prepare_temp_type const & prep)
    : statement_impl(prep.get_prepare_info()->session_),
    refCount_(1)
{
    ref_counted_prepare_info * prepInfo = prep.get_prepare_info();

        intos_.swap(prepInfo->intos_);
    uses_.swap(prepInfo->uses_);

        alloc();

        prepare(rewrite_for_procedure_call(prepInfo->get_query()));

    define_and_bind();
}
