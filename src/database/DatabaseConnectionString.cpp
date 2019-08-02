
#include "DatabaseConnectionString.h"

#include <regex>

namespace viichain
{
std::string
removePasswordFromConnectionString(std::string connectionString)
{
    std::string escapedSingleQuotePat("\\\\'");
    std::string nonSingleQuotePat("[^']");
    std::string singleQuotedStringPat("'(?:" + nonSingleQuotePat + "|" +
                                      escapedSingleQuotePat + ")*'");
    std::string bareWordPat("\\w+");
    std::string paramValPat("(?:" + bareWordPat + "|" + singleQuotedStringPat +
                            ")");
    std::string paramPat("(?:" + bareWordPat + " *= *" + paramValPat + " *)");
    std::string passwordParamPat("password( *= *)" + paramValPat);
    std::string connPat("^(" + bareWordPat + ":// *)(" + paramPat + "*)" +
                        passwordParamPat + "( *)(" + paramPat + "*)$");
    return std::regex_replace(connectionString, std::regex(connPat),
                              "$1$2password$3********$4$5");
}
}
