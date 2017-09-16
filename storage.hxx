#pragma once

#include "sqlite++/sqlite.hxx"
#include "json/json.h"

#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <iostream>

class Storage {
public:
  Storage();

  int setCompileCommand (const std::string & fileName,
                         const std::string & directory,
                         const std::vector<std::string> & args);

  void getCompileCommand (const std::string & fileName,
                          std::string & directory,
                          std::vector<std::string> & args);

  std::string nextFile ();

  void cleanIndex () ;

  Sqlite::Transaction beginTransaction () {
    return Sqlite::Transaction(db_);
  }

  bool beginFile (const std::string & fileName);

  void addInclude (const int includedId,
                   const int sourceId);

  void addInclude (const std::string & includedFile,
                   const std::string & sourceFile);

  void removeFile (const std::string & fileName);

  void addTag (const std::string & usr,
               const std::string & kind,
               const std::string & spelling,
               const std::string & fileName,
               const int line1, const int col1, const int offset1,
               const int line2, const int col2, const int offset2,
               bool isDeclaration, bool isVirtual,
               std::vector<const std::string> overriden_usrs);

  struct Reference {
    std::string file;
    int line1;
    int line2;
    int col1;
    int col2;
    int offset1;
    int offset2;
    std::string kind;
    std::string spelling;

    Json::Value json () const {
      Json::Value json;
      json["file"]     = file;
      json["line1"]    = line1;
      json["line2"]    = line2;
      json["col1"]     = col1;
      json["col2"]     = col2;
      json["offset1"]  = offset1;
      json["offset2"]  = offset2;
      json["kind"]     = kind;
      json["spelling"] = spelling;
      return json;
    }
  };

  struct Definition {
    std::string usr;
    std::string file;
    int line1;
    int line2;
    int col1;
    int col2;
    std::string kind;
    std::string spelling;
    int isVirtual;

    Json::Value json () const {
      Json::Value json;
      json["usr"]      = usr;
      json["file"]     = file;
      json["line1"]    = line1;
      json["line2"]    = line2;
      json["col1"]     = col1;
      json["col2"]     = col2;
      json["kind"]     = kind;
      json["spelling"] = spelling;
      json["isVirtual"] = isVirtual;
      return json;
    }
  };

  struct RefDef {
    Reference ref;
    Definition def;

    Json::Value json () const {
      Json::Value json;
      json["ref"] = ref.json();
      json["def"] = def.json();
      return json;
    }
  };

  std::vector<RefDef> findDefinition (const std::string fileName,
                       int offset);

  std::vector<Reference> grep (const std::string usr);

  void setOption (const std::string & name, const std::string & value);

  void setOption (const std::string & name, const std::vector<std::string> & value);

  std::string getOption (const std::string & name);

  struct Vector {};
  std::vector<std::string> getOption (const std::string & name, const Vector & v);

private:
  int fileId_ (const std::string & fileName);

  int addFile_ (const std::string & fileName);

  std::string serialize_ (const std::vector<std::string> & v);

  void deserialize_ (const std::string & s, std::vector<std::string> & v);

  Sqlite::Database db_;
  /*
  Sqlite::Statement preparedDeleteFileFromCommand;
  Sqlite::Statement preparedInsertIntoCommands;
  Sqlite::Statement preparedSelectCompileCommandFromCommands;
  Sqlite::Statement preparedDeleteFileFromCommand;
  Sqlite::Statement preparedDeleteFileFromCommand;
  Sqlite::Statement preparedDeleteFileFromCommand;
  Sqlite::Statement preparedDeleteFileFromCommand;
 */

};
