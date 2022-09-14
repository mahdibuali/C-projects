#ifndef shell_hh
#define shell_hh

#include "command.hh"

struct Shell {

  static void prompt();
  static std::vector<pid_t> back_pid;
  static Command _currentCommand;
  static int return_code;
  static pid_t last_pid;
  static std::string last_arg;
  static char * path;
};

#endif
