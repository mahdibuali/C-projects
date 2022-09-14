
/*
 * CS-252 hhhh
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE GREATGREAT GREATAMPERSAND GREATGREATAMPERSAND LESS AMPERSAND PIPE TWOGREAT

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"
#include <regex.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <errno.h>

std::vector<std::string> entries;
void yyerror(const char * s);
int yylex();
void expandWildcardsIfNecessary(std::string * argument);
void expandWildcards(char * pre, char * suff);
%}

%%

goal:
  commands
  ;

commands:
  command
  | commands command
  ;

command: command_line
       ;

simple_command:	
  command_and_args iomodifier_opt NEWLINE {
    printf("   Yacc: Execute command\n");
    Shell::_currentCommand.execute();
  }
  | NEWLINE 
  | error NEWLINE { yyerrok; }
  ;

command_line:
  pipe_list iomodifier_list background NEWLINE {
    //printf("   Yacc: Execute command\n");
    Shell::_currentCommand.execute();
  }
  | NEWLINE 
  | error NEWLINE { yyerrok; }
  ;


command_and_args:
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

pipe_list:
  pipe_list PIPE command_and_args
  | command_and_args
  ;

argument:
  WORD {
    //printf("   Yacc: insert argument \"%s\"\n", $1->c_str());
    // Command::_currentSimpleCommand->insertArgument( $1 );
    expandWildcardsIfNecessary($1);
    delete($1);
  }
  ;

command_word:
  WORD {
    //printf("   Yacc: insert command \"%s\"\n", $1->c_str());
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

iomodifier_opt:
  GREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile) {
      printf("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._outFile = $2;
    }
    
  }
  | LESS WORD {
    //printf("   Yacc: insert input \"%s\"\n", $2->c_str());
    Shell::_currentCommand._inFile = $2;
  }
  | GREATGREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile) {
      printf("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._append = true;
    }
  }
  | GREATAMPERSAND WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._twof = false;
  }
  | TWOGREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._errFile = $2;
  }
  | GREATGREATAMPERSAND WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
    Shell::_currentCommand._twof = false;
    Shell::_currentCommand._append = true;
  }
  ;

iomodifier_list:
  iomodifier_list iomodifier_opt
  | /* can be empty */ 
  ;

background:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | /* can be empty */ 
  ;


%%
void expandWildcardsIfNecessary(std::string * argument) {
  entries.clear();
  
  char * arg = (char *) argument->c_str();
  // printf("I exist: %s\n", arg);
  if (!strchr(arg, '?') && !strchr(arg, '*')) {
    std::string * a = new std::string(arg);
    Command::_currentSimpleCommand->insertArgument(a);
    return;
  }
  expandWildcards(NULL, arg);
  std::sort(entries.begin(), entries.end());
  if (entries.size() == 0) {
    std::string * a = new std::string(arg);
    Command::_currentSimpleCommand->insertArgument(a);
    return;
  }
  for (int k = 0; k < entries.size(); k++) {
    std::string * a = new std::string(entries[k]);
    // printf("hi: %s\n", a->c_str());
    Command::_currentSimpleCommand->insertArgument(a);
  }
}

void expandWildcards(char * pre, char * suff) {
  char * temp1;
  char * temp2;
  char * path;
  if (pre) {
    path = (char *) malloc(strlen(pre) + 10);
    temp1 = pre;
    temp2 = path;
    if (*pre != '/') {
      *temp2 = '.';
      temp2++;
      *temp2 = '/';
      temp2++; 
    }
    while (*temp1) {
      *temp2 = *temp1;
      temp2++;
      temp1++;
    }
    *temp2 = 0;
  } else {
    path = (char *) malloc(3);
    temp2 = path;
    if (*suff != '/') {
      *temp2 = '.';
      temp2++;
    }
    *temp2 = '/';
    temp2++;
    *temp2 = 0;
  }
  // printf("suff: %s\n", suff);

  char * arg = (char *) malloc(strlen(suff) + 10);
  temp1 = arg;
  temp2 = suff;
  bool s = false;
  if (*temp2 == '/') {
    s = true;
    temp2++;
  }
  while (*temp2 && *temp2 != '/') {
    *temp1 = *temp2;
    temp1++;
    temp2++;
  }
  *temp1 = 0;
  // printf("arg: %s\n", arg);
  if (strchr(arg, '?') || strchr(arg, '*')) {
    // printf("hi\n");
    // printf("arg: %s\n", arg);
    char * reg =  (char*) malloc( 2 * strlen(arg) + 10);
    char * a = arg;
    char * r = reg;
    *r = '^';
    r++;
    while (*a) {
      if (*a == '*') {
        *r = '.';
        r++;
        *r = '*';
        r++;
      } else if (*a == '?') {
        *r = '.';
        r++;
      } else if (*a == '.') {
        *r = '\\';
        r++;
        *r = '.';
        r++;
      } else {
        *r = *a;
        r++;
      }
      a++;
    }
    *r = '$';
    r++;
    *r = 0;
    regex_t re;	
    // printf("reg: %s\n", reg);
    int result = regcomp(&re, reg,  REG_EXTENDED|REG_NOSUB);
    
    if( result != 0 ) {
      perror("regcomp");
      return;
    }
    DIR * dir = opendir(path);
    if (!dir) {
      perror("opendir");
      return;
    } 
    free(path);
    struct dirent * ent;
    while ((ent = readdir(dir))) {
      if (regexec(&re, ent->d_name, 1, NULL, 0) == 0) {
        // printf("is: %d, %d\n", reg[1] == '\\' , reg[2] == '.');
        if (ent->d_name[0] != '.' || (reg[1] == '\\' && reg[2] == '.')) {
          if(*temp2) {
            if (ent->d_type == DT_DIR) {
              int k = 0;
              if (pre) {
                k = strlen(pre);
              }
              char * new_pre = (char *) malloc(k + strlen( ent->d_name) + 10);
              temp1 = new_pre;
              char * x = pre;
              if (x) {
                while (*x) {
                  *temp1 = *x;
                  temp1++;
                  x++;
                }
                *temp1 = '/';
                temp1++;
              } else {
                if(s) {
                  *temp1 = '/';
                  temp1++;
                }
              }
              for (int j = 0; j < strlen(ent->d_name); j++) {
                *temp1 = ent->d_name[j];
                temp1++;
              }
              *temp1 = 0;
              char * new_suff = (char *) malloc(strlen(temp2) + 1);
              temp1 = new_suff;
              char * temp3 = temp2;
              while(*temp3) {
                *temp1 = *temp3;
                temp1++;
                temp3++;
              }
              *temp1 = 0;
              // printf("A thing: %s, %s\n", new_pre, new_suff);
              expandWildcards(new_pre, new_suff);
              free(new_suff);
              free(new_pre);

            }
          } else {
            char * ff = strdup(ent->d_name);
            std::string * a = new std::string(ff);
            if (pre) {
              std::string * b = new std::string(pre);
              std::string * c = new std::string("/");
              *a = *b + *c + *a;
              delete(b);
              delete(c);
            }
            entries.push_back(*a);
            free(ff);
            delete(a);
          }
        }
      }
    }

    
    free(reg);
    closedir(dir);
    regfree(&re);
  } else {
    if(*temp2) {
      int k = 0;
      if (pre) {
        k = strlen(pre);
      }
      char * new_pre = (char *) malloc(k + strlen(arg) + 10);
      temp1 = new_pre;
      char * x = pre;
      if (x) {
        while (*x) {
          *temp1 = *x;
          temp1++;
          x++;
        }
        *temp1 = '/';
        temp1++;
      } else {
        if(s) {
          *temp1 = '/';
          temp1++;
        }
      }
      for (int j = 0; j < strlen(arg); j++) {
        *temp1 = arg[j];
        temp1++;
      }
      *temp1 = 0;
      char * new_suff = (char *) malloc(strlen(temp2) + 1);
      temp1 = new_suff;
      while(*temp2) {
        *temp1 = *temp2;
        temp1++;
        temp2++;
      }
      *temp1 = 0;
      // printf("Nothing: %s, %s\n", new_pre, new_suff);
      expandWildcards(new_pre, new_suff);
      free(new_suff);
      free(new_pre);
    } else {
      std::string * a = new std::string(arg);
      if (pre) {
        std::string * b = new std::string(pre);
        std::string * c = new std::string("/");
        *a = *b + *c + *a;
        delete(b);
        delete(c);
      }
      entries.push_back(*a);
      delete(a);
    }
  }
  free(arg);
}
void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
