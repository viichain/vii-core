#pragma once


#include <memory>
#include <string>

namespace viichain
{
class TmpDir
{
    std::unique_ptr<std::string> mPath;

  public:
    TmpDir(std::string const& prefix);
    TmpDir(TmpDir&&);
    ~TmpDir();
    std::string const& getName() const;
};

class TmpDirManager
{
    std::string mRoot;
    void clean();

  public:
    TmpDirManager(std::string const& root);
    ~TmpDirManager();
    TmpDir tmpDir(std::string const& prefix);
};
}
