#pragma once


#include <functional>
#include <string>
#include <vector>

namespace viichain
{
namespace fs
{


void lockFile(std::string const& path);
void unlockFile(std::string const& path);

bool exists(std::string const& path);

void deltree(std::string const& path);

bool mkdir(std::string const& path);

bool mkpath(std::string const& path);

std::vector<std::string>
findfiles(std::string const& path,
          std::function<bool(std::string const& name)> predicate);

size_t size(std::ifstream& ifs);

size_t size(std::string const& path);

class PathSplitter
{
  public:
    explicit PathSplitter(std::string path);

    std::string next();
    bool hasNext() const;

  private:
    std::string mPath;
    std::string::size_type mPos;
};


std::string hexStr(uint32_t checkpointNum);

std::string hexDir(std::string const& hexStr);

std::string baseName(std::string const& type, std::string const& hexStr,
                     std::string const& suffix);

std::string remoteDir(std::string const& type, std::string const& hexStr);

std::string remoteName(std::string const& type, std::string const& hexStr,
                       std::string const& suffix);

void checkGzipSuffix(std::string const& filename);

void checkNoGzipSuffix(std::string const& filename);

int getMaxConnections();
}
}
