#ifndef DBDCONF_CLI_H
#define DBDCONF_CLI_H
#include <glib.h>

typedef enum DbdCliInstanceCommand_t {
    DBD_INSTANCE_COMMAND_NONE = 0,
    DBD_INSTANCE_COMMAND_HELP, // dbdconf help | dbdconf <gvdb_file>? COMMAND help
    DBD_INSTANCE_COMMAND_DUMP, // dbdconf <gvdb_file> dump <dir> | dbdconf dump <gvdb_file> <dir>
    DBD_INSTANCE_COMMAND_LIST, // dbdconf <gvdb_file> list <dir> | dbdconf list <gvdb_file> <dir>
    DBD_INSTANCE_COMMAND_READ, // dbdconf <gvdb_file> read <key> | dbdconf read <gvdb_file> <key>
} DbdCliInstanceCommand;

typedef struct DbdCliInstance_t {
    DbdCliInstanceCommand command;
    const gchar *gvdb_file;
    const gchar *path;
    // For future(write support).
    const gchar *value;
} DbdCliInstance;

DbdCliInstance* dbd_parse_args(int argc, const char** argv);
void dbd_free_args(DbdCliInstance* instance);

#endif // DBDCONF_CLI_H