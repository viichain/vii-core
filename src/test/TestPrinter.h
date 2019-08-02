#pragma once


#include "catchup/CatchupWork.h"
#include "history/test/HistoryTestsUtils.h"
#include "lib/catch.hpp"
#include "xdrpp/printer.h"
#include "xdrpp/types.h"

namespace viichain
{
struct OfferState;
}

namespace Catch
{
template <typename T>
struct StringMaker<T, typename std::enable_if<xdr::xdr_traits<T>::valid>::type>
{
    static std::string
    convert(T const& val)
    {
        return xdr::xdr_to_string(val);
    }
};

template <> struct StringMaker<viichain::OfferState>
{
    static std::string convert(viichain::OfferState const& os);
};

template <> struct StringMaker<viichain::CatchupRange>
{
    static std::string convert(viichain::CatchupRange const& cr);
};

template <> struct StringMaker<viichain::historytestutils::CatchupPerformedWork>
{
    static std::string
    convert(viichain::historytestutils::CatchupPerformedWork const& cr);
};
}
