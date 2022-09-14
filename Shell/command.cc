/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include "command.hh"
#include "shell.hh"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void yyrestart(FILE * file);
int yyparse(void);

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
    _inter = false;
    _twof = true;
    _source = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile && _twof) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
    _append = false;
    _inter = false;
    _twof = true;
    
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

char ** Command::c_array(SimpleCommand *vec) {
    char **c = new char*[vec->_arguments.size() + 1];
    for (int i = 0; i < vec->_arguments.size(); i++) {
        c[i] = (char *) vec->_arguments[i]->c_str();
    }
    c[vec->_arguments.size()] = NULL;
    for (int i = 0; i < vec->_arguments.size(); i++) {
        //printf("%s\n", c[i]);
    }
    return c;
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        if (isatty(0)) {
            Shell::prompt();
        }
        return;
    }

    if ( _simpleCommands.size() == 1 && _simpleCommands[0]->_arguments.size() == 1) {
        std::string * c = _simpleCommands[0]->_arguments[0];
        if (c->compare("exit" ) == 0) {
			printf( "Bye!\n");
			exit( 1 );
		}
    }
    
    // if (_simpleCommands[0]->_arguments[0]->compare("setenv" ) == 0) {
    //     int error = setenv(_simpleCommands[0]->_arguments[1]->c_str(), _simpleCommands[0]->_arguments[1]->c_str(), 1);
    //     if(error) {
	// 		perror("setenv");
	// 	}
    //     clear();
    //     // Print new prompt
    //     Shell::prompt();
        
    //     return;
    // }

    // if (_simpleCommands[0]->_arguments[0]->compare("unsetenv" ) == 0) {
    //     unsetenv(_simpleCommands[0]->_arguments[1]->c_str());
    //     clear();
    //     // Print new prompt
    //     if (isatty(0)) {
    //         Shell::prompt();
    //     }
    //     return;
    // }

    // if (_simpleCommands[0]->_arguments[0]->compare("cd" ) == 0) {
    //     if (_simpleCommands[0]->_arguments.size() == 1) {
    //         chdir(getenv("HOME"));
    //     } else {
    //         chdir(_simpleCommands[0]->_arguments[1]->c_str());
    //     }
    //     clear();
    //     // Print new prompt
    //     if (isatty(0)) {
    //         Shell::prompt();
    //     }
    //     return;
    // }

    // if (_simpleCommands[0]->_arguments[0]->compare("source" ) == 0) {
    //     FILE * f = fopen(_simpleCommands[0]->_arguments[1]->c_str(), "r");
    //     yyrestart(f);
    //     yyparse();
    //     yyrestart(stdin);
    //     fclose(f);

    //     clear();
    //     // Print new prompt
    //     if (isatty(0)) {
    //         Shell::prompt();
    //     }
    //     return;
    // }
    
    // Print contents of Command data structure
    // print();

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec
    int tmpin = dup(0);
    int tmpout = dup(1);
    int tmperr = dup(2);
    
    //set input
    int fdin;
    if (_inFile) {
        fdin = open(_inFile->c_str(), O_RDONLY);
    } else {
        fdin = dup(tmpin);
    }
    _inter = true;

    int ret;
    int fdout;
    int fderr;
    for ( int i = 0; i < _simpleCommands.size(); i++) {
        //redirect output
        dup2(fdin, 0);
        close(fdin);
        //setup output
        if (i == _simpleCommands.size() - 1) {
            if (_outFile) {
                if (_append) {
                    fdout = open(_outFile->c_str(), O_CREAT|O_WRONLY|O_APPEND, 0664);
                } else {
                    fdout = open(_outFile->c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0664);
                }
            } else {
                fdout = dup(tmpout);
            }

            if (_errFile) {
                if (_append) {
                    fderr = open(_errFile->c_str(), O_CREAT|O_WRONLY|O_APPEND, 0664);
                } else {
                    fderr = open(_errFile->c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0664);
                }
            } else {
                fderr = dup(tmperr);
            }
            dup2(fderr, 2);
            close(fderr);
        } else {
            int fdpipe[2];
            pipe(fdpipe);
            fdout = fdpipe[1];
            fdin = fdpipe[0];
        }

        dup2(fdout, 1);
        close(fdout);
        
        if (_simpleCommands[i]->_arguments[0]->compare("setenv" ) == 0) {
            const char * a = (const char *) _simpleCommands[i]->_arguments[1]->c_str();
            const char * b = (const char *) _simpleCommands[i]->_arguments[2]->c_str();
            setenv(a, b, 1);
            continue;
        }

        if (_simpleCommands[i]->_arguments[0]->compare("unsetenv" ) == 0) {
            unsetenv(_simpleCommands[i]->_arguments[1]->c_str());
            continue;
        }

        if (_simpleCommands[i]->_arguments[0]->compare("cd" ) == 0) {
            int e = 0;
            if (_simpleCommands[i]->_arguments.size() == 1) {
                    e = chdir(getenv("HOME"));
                } else {
                    e = chdir(_simpleCommands[i]->_arguments[1]->c_str());
                }
                if (e < 0) {
                    fprintf(stderr, "cd: can't cd to %s\n", _simpleCommands[i]->_arguments[1]->c_str());
                }
                continue;
        }

        ret = fork();
        if (ret == 0) {
            if (_simpleCommands[i]->_arguments[0]->compare("printenv" ) == 0) {
                char ** var = environ;
                while (*var) {
                    printf("%s\n", *var);
                    var++;
                }   
                exit(0);
            } else {
                // for (int j = 0; j < _simpleCommands[i]->_arguments.size(); j++) {
                //     fprintf(stdout, "cmd: %s\n", _simpleCommands[i]->_arguments[j]->c_str());
                // }
                char ** args = c_array(_simpleCommands[i]);
                printf("arg:%s\b", args[0]);
                execvp(args[0], args);
                perror("execvp");
                _exit(1);
            }
        } else if (ret < 0) { 
            perror("fork");
            return;
        }
    }
    int k = _simpleCommands[_simpleCommands.size() - 1]->_arguments.size();
    Shell::last_arg.assign(*(_simpleCommands[_simpleCommands.size() - 1]->_arguments[k- 1]));
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    dup2(tmperr, 2);
    close(tmperr);
    close(tmpin);
    close(tmpout);
    int status;
    if (!_background) { 
    // wait for last process 
        waitpid(ret, &status, 0);
        Shell::return_code = WEXITSTATUS(status);
    } else {
        Shell::back_pid.push_back(ret);
    }

    // Clear to prepare for next command
    clear();
    // Print new prompt
    if (isatty(0) && !_source) {
        Shell::prompt();
    }
    
    
    
}

SimpleCommand * Command::_currentSimpleCommand;
