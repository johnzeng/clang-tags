#include "storage.hxx"

Storage::Storage()
    : db_(".ct.sqlite")
{
    db_.execute ("CREATE TABLE IF NOT EXISTS files ("
            "  id      INTEGER PRIMARY KEY,"
            "  name    TEXT,"
            "  indexed INTEGER"
            ")");
    db_.execute ("CREATE TABLE IF NOT EXISTS commands ("
            "  fileId     INTEGER REFERENCES files(id),"
            "  directory  TEXT,"
            "  args       TEXT"
            ")");
    db_.execute ("CREATE TABLE IF NOT EXISTS includes ("
            "  sourceId   INTEGER REFERENCES files(id),"
            "  includedId INTEGER REFERENCES files(id)"
            ")");
    db_.execute ("CREATE TABLE IF NOT EXISTS tags ("
            "  fileId   INTEGER REFERENCES files(id),"
            "  usr      TEXT,"
            "  kind     TEXT,"
            "  spelling TEXT,"
            "  line1    INTEGER,"
            "  col1     INTEGER,"
            "  offset1  INTEGER,"
            "  line2    INTEGER,"
            "  col2     INTEGER,"
            "  offset2  INTEGER,"
            "  isDecl   BOOLEAN,"
            "  isVirtual BOOLEAN"
            ")");
    db_.execute ("CREATE TABLE IF NOT EXISTS overriden_methods("
            "  usr TEXT REFERENCES tags(usr),"
            "  overriden_usr TEXT"
            ")");
    db_.execute ("CREATE TABLE IF NOT EXISTS options ( "
            "  name   TEXT, "
            "  value  TEXT "
            ")");

    //build indexes
    db_.execute ("CREATE INDEX IF NOT EXISTS usr_offset_fileId_index ON tags (usr, offset1, offset2, fileId)");
    db_.execute ("CREATE INDEX IF NOT EXISTS usr_index ON tags (usr)");
    db_.execute ("CREATE INDEX IF NOT EXISTS name_index ON options (name)");
    db_.execute ("CREATE INDEX IF NOT EXISTS name_index ON files (name)");
}


int Storage::setCompileCommand (const std::string & fileName,
        const std::string & directory,
        const std::vector<std::string> & args) {
    int fileId = addFile_ (fileName);
    addInclude (fileId, fileId);

        db_.prepare("DELETE FROM commands "
            "WHERE fileId=?").bind (fileId)
        .step();

    db_.prepare ("INSERT INTO commands VALUES (?,?,?)")
        .bind (fileId)
        .bind (directory)
        .bind (serialize_ (args))
        .step();

    return fileId;
}

void Storage::getCompileCommand (const std::string & fileName,
        std::string & directory,
        std::vector<std::string> & args) {

    int fileId = fileId_ (fileName);
    Sqlite::Statement stmt
        = db_.prepare ("SELECT commands.directory, commands.args "
                "FROM includes "
                "INNER JOIN commands ON includes.sourceId = commands.fileId "
                "WHERE includes.includedId = ?")
        .bind (fileId);

    switch (stmt.step()) {
        case SQLITE_DONE:
            throw std::runtime_error ("no compilation command for file `"
                    + fileName + "'");
            break;
        default:
            std::string serializedArgs;
            stmt >> directory >> serializedArgs;
            deserialize_ (serializedArgs, args);
    }
}

std::string Storage::nextFile () {
    Sqlite::Statement stmt
        = db_.prepare ("SELECT included.name, included.indexed, source.name, "
                "       count(source.name) AS sourceCount "
                "FROM includes "
                "INNER JOIN files AS source ON source.id = includes.sourceId "
                "INNER JOIN files AS included ON included.id = includes.includedId "
                "GROUP BY included.id "
                "ORDER BY sourceCount ");
    while (stmt.step() == SQLITE_ROW) {
        std::string includedName;
        int indexed;
        std::string sourceName;
        stmt >> includedName >> indexed >> sourceName;

        struct stat fileStat;
        if (stat (includedName.c_str(), &fileStat) != 0) {
            std::cerr << "Warning: could not stat() file `" << includedName << "'" << std::endl
                << "  removing it from the index" << std::endl;
            removeFile (includedName);
            continue;
        }
        int modified = fileStat.st_mtime;

        if (modified > indexed) {
            return sourceName;
        }
    }

    return "";
}

void Storage::cleanIndex () {
    db_.execute ("DELETE FROM tags");
    db_.execute ("UPDATE files SET indexed = 0");
}

bool Storage::beginFile (const std::string & fileName) {
    int fileId = addFile_ (fileName);

    int indexed;
    {
        Sqlite::Statement stmt
            = db_.prepare ("SELECT indexed FROM files WHERE id = ?")
            .bind (fileId);
        stmt.step();
        stmt >> indexed;
    }

    struct stat fileStat;
    stat (fileName.c_str(), &fileStat);
    int modified = fileStat.st_mtime;

    if (modified > indexed) {
        db_.prepare ("DELETE FROM tags WHERE fileId=?").bind (fileId).step();
        db_.prepare ("DELETE FROM includes WHERE sourceId=?").bind (fileId).step();
        db_.prepare ("UPDATE files "
                "SET indexed=? "
                "WHERE id=?")
            .bind (modified)
            .bind (fileId)
            .step();
        return true;
    } else {
        return false;
    }
}

void Storage::addInclude (const int includedId,
        const int sourceId)
{
    int res = db_.prepare ("SELECT * FROM includes "
            "WHERE sourceId=? "
            "  AND includedId=?")
        .bind (sourceId) .bind (includedId)
        .step ();
    if (res == SQLITE_DONE) { // No matching row
        db_.prepare ("INSERT INTO includes VALUES (?,?)")
            .bind (sourceId) . bind (includedId)
            .step();
    }
}

void Storage::addInclude (const std::string & includedFile,
        const std::string & sourceFile) {
    int includedId = fileId_ (includedFile);
    int sourceId   = fileId_ (sourceFile);
    if (includedId == -1 || sourceId == -1)
        throw std::runtime_error ("Cannot add inclusion for unknown files `"
                + includedFile + "' and `" + sourceFile + "'");
    addInclude (includedId, sourceId);
}

void Storage::removeFile (const std::string & fileName) {
    int fileId = fileId_ (fileName);
    db_
        .prepare ("DELETE FROM commands WHERE fileId = ?")
        .bind (fileId)
        .step();

    db_
        .prepare ("DELETE FROM includes WHERE sourceId = ?")
        .bind (fileId)
        .step();

    db_
        .prepare ("DELETE FROM includes WHERE includedId = ?")
        .bind (fileId)
        .step();

    db_
        .prepare ("DELETE FROM tags WHERE fileId = ?")
        .bind (fileId)
        .step();

    db_.prepare ("DELETE FROM files WHERE id = ?")
        .bind (fileId)
        .step();
}

void Storage::addTag (const std::string & usr,
        const std::string & kind,
        const std::string & spelling,
        const std::string & fileName,
        const int line1, const int col1, const int offset1,
        const int line2, const int col2, const int offset2,
        bool isDeclaration, bool isVirtual,
        const std::vector<std::string> overriden_usrs) {
    int fileId = fileId_ (fileName);
    if (fileId == -1) {
        return;
    }

    Sqlite::Statement stmt =
        db_.prepare ("SELECT * FROM tags "
                "WHERE fileId=? "
                "  AND usr=?"
                "  AND offset1=?"
                "  AND offset2=?")
        .bind (fileId).bind (usr).bind (offset1).bind (offset2);
    if (stmt.step() == SQLITE_DONE) { // no matching row
        db_.prepare ("INSERT INTO tags VALUES (?,?,?,?,?,?,?,?,?,?,?,?)")
            .bind(fileId) .bind(usr)  .bind(kind)    .bind(spelling)
            .bind(line1)  .bind(col1) .bind(offset1)
            .bind(line2)  .bind(col2) .bind(offset2)
            .bind(isDeclaration) .bind(isVirtual)
            .step();
        if(isVirtual)
        {
            for(auto it = overriden_usrs.begin(); it < overriden_usrs.end(); it++)
            {
                db_.prepare("INSERT INTO overriden_methods VALUES(?,?)")
                    .bind(usr).bind(*it).step();
            }
        }
    }
}

std::vector<Storage::RefDef> Storage::findOverridenDefinition (const std::string fileName, const std::string usr)
{
    Sqlite::Statement stmt =
        db_.prepare ("SELECT def.offset1, def.offset2, def.kind, def.spelling,"
                "       overriden_methods.overriden_usr, defFile.name,"
                "       def.line1, def.line2, def.col1, def.col2, "
                "       def.kind, def.spelling, def.isVirtual "
                "FROM overriden_methods "
                "INNER JOIN tags AS def ON def.usr = overriden_methods.usr "
                "INNER JOIN files AS defFile ON def.fileId = defFile.id "
                "WHERE overriden_methods.usr = ? "
                "GROUP BY overriden_methods.overriden_usr "
                "ORDER BY (def.offset2 - def.offset1)")
        .bind (usr);
    std::vector<Storage::RefDef> ret;
    while (stmt.step() == SQLITE_ROW) {
        Storage::RefDef refDef;
        Storage::Reference & ref = refDef.ref;
        Definition & def = refDef.def;

        stmt >> ref.offset1 >> ref.offset2 >> ref.kind >> ref.spelling
            >> def.usr >> def.file
            >> def.line1 >> def.line2 >> def.col1 >> def.col2
            >> def.kind >> def.spelling >> def.isVirtual;
        ref.file = fileName;
        ret.push_back(refDef);
    }
    return ret;
}

std::vector<Storage::RefDef> Storage::findDefinition (const std::string fileName,
        int offset) {
    int fileId = fileId_ (fileName);
    Sqlite::Statement stmt =
        db_.prepare ("SELECT ref.offset1, ref.offset2, ref.kind, ref.spelling,"
                "       def.usr, defFile.name,"
                "       def.line1, def.line2, def.col1, def.col2, "
                "       def.kind, def.spelling, def.isVirtual "
                "FROM tags AS ref "
                "INNER JOIN tags AS def ON def.usr = ref.usr "
                "INNER JOIN files AS defFile ON def.fileId = defFile.id "
                "WHERE def.isDecl = 1 "
                "  AND ref.fileId = ?  "
                "  AND ref.offset1 <= ? "
                "  AND ref.offset2 >= ? "
                "ORDER BY (ref.offset2 - ref.offset1)")
        .bind (fileId)
        .bind (offset)
        .bind (offset);

    std::vector<Storage::RefDef> ret;
    while (stmt.step() == SQLITE_ROW) {
        Storage::RefDef refDef;
        Storage::Reference & ref = refDef.ref;
        Definition & def = refDef.def;

        stmt >> ref.offset1 >> ref.offset2 >> ref.kind >> ref.spelling
            >> def.usr >> def.file
            >> def.line1 >> def.line2 >> def.col1 >> def.col2
            >> def.kind >> def.spelling >> def.isVirtual;
        ref.file = fileName;
        ret.push_back(refDef);
    }
    return ret;
}

std::vector<Storage::Reference> Storage::findOverridenDefinition(const std::string usr) {
    Sqlite::Statement stmt =
        db_.prepare("SELECT ref.line1, ref.line2, ref.col1, ref.col2, "
                "       ref.offset1, ref.offset2, refFile.name, ref.kind "
                "FROM overriden_methods "
                "INNER JOIN tags AS ref ON overriden_methods.overriden_usr = ref.usr "
                "INNER JOIN files AS refFile ON ref.fileId = refFile.id "
                "WHERE overriden_methods.usr = ? "
                "GROUP BY overriden_methods.overriden_usr")
        .bind (usr);

    std::vector<Storage::Reference> ret;
    while (stmt.step() == SQLITE_ROW) {
        Storage::Reference ref;
        stmt >> ref.line1 >> ref.line2 >> ref.col1 >> ref.col2
            >> ref.offset1 >> ref.offset2 >> ref.file >> ref.kind;
        ret.push_back (ref);
    }
    return ret;
}

std::vector<Storage::Reference> Storage::grep (const std::string usr) {
    Sqlite::Statement stmt =
        db_.prepare("SELECT ref.line1, ref.line2, ref.col1, ref.col2, "
                "       ref.offset1, ref.offset2, refFile.name, ref.kind "
                "FROM tags AS ref "
                "INNER JOIN files AS refFile ON ref.fileId = refFile.id "
                "WHERE ref.usr = ?")
        .bind (usr);

    std::vector<Storage::Reference> ret;
    while (stmt.step() == SQLITE_ROW) {
        Storage::Reference ref;
        stmt >> ref.line1 >> ref.line2 >> ref.col1 >> ref.col2
            >> ref.offset1 >> ref.offset2 >> ref.file >> ref.kind;
        ret.push_back (ref);
    }
    return ret;
}

void Storage::setOption (const std::string & name, const std::string & value) {
    db_.prepare ("DELETE FROM options "
            "WHERE name = ?")
        .bind (name)
        .step();

    db_.prepare ("INSERT INTO options "
            "VALUES (?, ?)")
        .bind (name)
        .bind (value)
        .step();
}

void Storage::setOption (const std::string & name, const std::vector<std::string> & value) {
    setOption (name, serialize_(value));
}


std::string Storage::getOption (const std::string & name) {
    Sqlite::Statement stmt =
        db_.prepare ("SELECT value FROM options "
                "WHERE name = ?")
        .bind (name);

    std::string ret;
    if (stmt.step() == SQLITE_ROW) {
        stmt >> ret;
    } else {
        throw std::runtime_error ("No stored value for option: `" + name + "'");
    }

    return ret;
}

std::vector<std::string> Storage::getOption (const std::string & name, const Vector & v) {
    std::vector<std::string> ret;
    deserialize_ (getOption (name), ret);
    return ret;
}

int Storage::fileId_ (const std::string & fileName) {
    Sqlite::Statement stmt
        = db_.prepare ("SELECT id FROM files WHERE name=?")
        .bind (fileName);

    int id = -1;
    if (stmt.step() == SQLITE_ROW) {
        stmt >> id;
    }

    return id;
}

int Storage::addFile_ (const std::string & fileName) {
    int id = fileId_ (fileName);
    if (id == -1) {
        db_.prepare ("INSERT INTO files VALUES (NULL, ?, 0)")
            .bind (fileName)
            .step();

        id = db_.lastInsertRowId();
    }
    return id;
}

std::string Storage::serialize_ (const std::vector<std::string> & v) {
    Json::Value json;
    auto it = v.begin();
    auto end = v.end();
    for ( ; it != end ; ++it) {
        json.append (*it);
    }

    Json::FastWriter writer;
    return writer.write (json);
}

void Storage::deserialize_ (const std::string & s, std::vector<std::string> & v) {
    Json::Value json;
    Json::Reader reader;
    if (! reader.parse (s, json)) {
        throw std::runtime_error (reader.getFormattedErrorMessages());
    }

    for (unsigned int i=0 ; i<json.size() ; ++i) {
        v.push_back (json[i].asString());
    }
}
