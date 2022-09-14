
/*
 *
 * CS-252 sh 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{
#include <pwd.h>
#include <cstring>
#include<algorithm>
#include "y.tab.hh"
#include "shell.hh"
#include <unistd.h>

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char c;

  if (!isatty(0)) {
    return getc(f);
  }

  if (!p || *p == 0) {
    char * s = read_line();
    p = s;
  }

  c = *p;
  p++;

  return c;
}

#undef getc
#define getc(f) mygetc(f)

static void yyunput (int c,char *buf_ptr);

void myunputc(int c) {
  unput(c);
}

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

"<" {
  return LESS;
}

">>" {
  return GREATGREAT;
}

">&" {
  return GREATAMPERSAND;
}

">>&" {
  return GREATGREATAMPERSAND;
}

"2>" {
  return TWOGREAT;
}

"|" {
  return PIPE;
}

"&" {
  return AMPERSAND;
}

\~([^ \t \n \/]*\/?)* {
  std::string * text = new std::string(yytext);
  std::size_t end = text->find("/");
  if (text->length() == 1) {
    yylval.cpp_string = new std::string(getenv("HOME"));  
  } else if (end != std::string::npos) {
    std::string user = text->substr(1, end - 1);
    * text = getpwnam(user.c_str())->pw_dir + text->substr(end, text->length() - end + 1);
    yylval.cpp_string = new std::string(*text);
  } else {
    std::string user = text->substr(1, text->length() - 1);
    yylval.cpp_string = new std::string(getpwnam(user.c_str())->pw_dir);
  }
  return WORD;
}
\$\{\$\} {
  std::string var = std::to_string(getpid());
  yylval.cpp_string = new std::string(var);
  return WORD;
}

\$\{\?\} {
  std::string var = std::to_string(Shell::return_code);
  yylval.cpp_string = new std::string(var);
  return WORD;
}

\$\{\!\} {
  std::string var = std::to_string(Shell::last_pid);
  yylval.cpp_string = new std::string(var);
  return WORD;
}
\$\{\_\} {
  yylval.cpp_string = new std::string(Shell::last_arg);
  return WORD;
}
\$\{SHELL\} {
  yylval.cpp_string = new std::string(Shell::path);
  return WORD;
}

[^ \t \n \$]*\$\{[^\n \} \t]*\}([^ \t \n \$]*\$\{[^\n \} \t]*\})*[^ \t \n \$]* {
  std::string * text = new std::string(yytext);
  std::size_t start = text->find("${");
  std::size_t end = text->find("}");
  while (start != std::string::npos) {
    char * var = getenv(text->substr(start + 2, end - start - 2).c_str());
    * text = text->substr(0, start) + var + text->substr(end + 1, text->length() - end - 1);
    start = text->find("${");
    end = text->find("}");
  }
  yylval.cpp_string = new std::string(*text);
  return WORD;
}


\$\([^\n]*\) {
  std::string * text = new std::string(yytext);
  std::string com = text->substr(2, text->length() - 3);

  int tmpin = dup(0);
  int tmpout = dup(1);
  int pin[2];
  int pout[2];
  pipe(pin);
  pipe(pout);
  dup2(pin[0], 0);
  close(pin[0]);
  dup2(pout[1], 1);
  close(pout[1]);

  write(pin[1], com.c_str(), com.size());
  write(pin[1], "\n",  1);
  write(pin[1], "exit\n", 5);

  close(pin[1]);

  int ret = fork();
  if (ret == 0) {
    execvp("/proc/self/exe", NULL);
    perror("execvp");
    _exit(1);
  } else if (ret < 0) { 
    perror("fork");
    exit(0);
  }
  dup2(tmpin, 0);
  dup2(tmpout, 1);
  close(tmpout);
  close(tmpin);

  char c;
  std::string buff = "";
  while (read(pout[0], &c, 1)) {
    // printf("char:%c\n", c);
    if (c == '\n') {
      buff.push_back(' ');
    } else {
      buff.push_back(c);
    }
  }
  close(pout[0]);
  unput('\0');
  for (int i = buff.length() - 7; i >= 0; i--) {
    unput(buff.c_str()[i]);
  }
}

(source)[^\n]* {
  std::string * text = new std::string(yytext);
  std::string path = text->substr(7, text->length() - 7);
  FILE* f = fopen(path.c_str(), "r");
  
  if (!f) {
    perror("file");
    if (isatty(0)) {
      Shell::prompt();
    }
  } else {
    Shell::_currentCommand._source = true;
    yypush_buffer_state(yy_create_buffer(f, YY_BUF_SIZE));
    yyparse();
    yypop_buffer_state();
    fclose(f);
    Shell::_currentCommand._source = false;
    if (isatty(0)) {
      Shell::prompt();
    }
  }
} 


\"[^\n\"]*\" {
  std::string * text = new std::string(yytext);
  yylval.cpp_string = new std::string(text->substr(1, text->length() - 2));
  return WORD;
}

[^ \n\t]*\\[^\n\t][^ \t\n]* {
  std::string * text = new std::string(yytext);
  std::string t = "";
  for (int i = 0; i < text->length(); i++) {
    if (text->c_str()[i] == '\\') {
      t.push_back(text->c_str()[i + 1]);
      i++;
    } else {
      t.push_back(text->c_str()[i]);
    }
  }
  // printf("Before:%s, After:%s\n", text->c_str(), t.c_str());
  yylval.cpp_string = new std::string(t);
  return WORD;
}

[^ \t\n\>\<\|\&][^ \t\n\>\<\|\&]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
  
}


