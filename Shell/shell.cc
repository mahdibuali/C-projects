#include <cstdio>

#include "shell.hh"
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include "shell.hh"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


extern "C" void disp( int sig ) {
  if (sig == SIGINT) {
    printf("\n");
    if (!Shell::_currentCommand._inter) {
      printf("myshell>");
      fflush(stdout);
    }
    Shell::_currentCommand.clear();
    
    
  }
  if (sig == SIGCHLD) {
    pid_t pid = waitpid(-1, NULL, WNOHANG);
    Shell::last_pid = pid;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    for (int i = 0; i < Shell::back_pid.size(); i++) {
      if (Shell::back_pid.at(i) == pid) {
        Shell::back_pid.erase(Shell::back_pid.begin() + i);
        printf("%d exited.\n", pid);
      }
      
    }
    
  }
  
}
typedef struct yy_buffer_state *YY_BUFFER_STATE;
void yyrestart(FILE * file);
int yyparse(void);
int yyparse(void);
void yypush_buffer_state(YY_BUFFER_STATE buffer);
void yypop_buffer_state();
YY_BUFFER_STATE yy_create_buffer(FILE * file, int size);


void Shell::prompt() {
  printf("myshell>");
  fflush(stdout);
}

int main(int argc, char *argv[]) {
  Shell::path = realpath(argv[0], NULL);
  struct sigaction sa;
  sa.sa_handler = disp;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if(sigaction(SIGINT, &sa, NULL)){
    perror("sigaction");
    exit(2);
  }

  if(sigaction(SIGCHLD, &sa, NULL)){
    perror("sigaction");
    exit(2);
  }

  FILE* f = fopen("~/.shellrc", "r");
  
  if (f) {
    Shell::_currentCommand._source = true;
    yypush_buffer_state(yy_create_buffer(f, 40000));
    yyparse();
    yypop_buffer_state();
    fclose(f);
    Shell::_currentCommand._source = false;
  }

  if (isatty(0)) {
    Shell::prompt();
  }

  yyparse();
}

Command Shell::_currentCommand;
std::vector<pid_t> Shell::back_pid;
int Shell::return_code;
pid_t Shell::last_pid;
std::string Shell::last_arg;
char * Shell::path;
