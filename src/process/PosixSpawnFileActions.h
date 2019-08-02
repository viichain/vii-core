#pragma once


#ifndef _WIN32

#include <string>

#include <spawn.h>
#include <sys/wait.h>

namespace viichain
{

class PosixSpawnFileActions
{
  public:
    PosixSpawnFileActions() = default;
    ~PosixSpawnFileActions();

    void addOpen(int fildes, std::string const& fileName, int oflag,
                 mode_t mode);

    operator posix_spawn_file_actions_t*();

  private:
    posix_spawn_file_actions_t mFileActions;
    bool mInitialized{false};

    void initialize();
};
}

#endif
