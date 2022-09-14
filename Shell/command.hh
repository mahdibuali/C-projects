#ifndef command_hh
#define command_hh

#include "simpleCommand.hh"

// Command Data Structure

struct Command {
  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;
  bool _background;
  bool _append;
  bool _inter;
  bool _twof;
  bool _source;

  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  char ** c_array(SimpleCommand *vec);
  void clear();
  void print();
  void execute();

  static SimpleCommand *_currentSimpleCommand;
};


#endif
