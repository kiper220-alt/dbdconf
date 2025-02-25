#include <cli.h>

static const char *DEFAULT_HELP_MESSAGE =
        "dbdconf - a small program for reading dconf layer files (GVDB)\n"
        "Usage:\n"
        "  dbdconf <GVDB_PATH> COMMAND [ARGS...]\n"
        "  dbdconf COMMAND <GVDB_PATH> [ARGS...]\n\n"
        "Commands:\n"
        "  help\t\tShow this information\n"
        "  read\t\tRead the value of a key\n"
        "  list\t\tList the contents of a dir\n"
        "  dump\t\tDump an entire subpath to stdout\n";

static const char *READ_HELP_MESSAGE =
        "Usage:\n"
        "  dbdconf GVDB_PATH read KEY\n"
        "  dbdconf read GVDB_PATH KEY\n\n"
        "Read the value of a key\n\n"
        "Arguments:\n"
        " GVDB_PATH\t\tA GVDB layer file path\n"
        " KEY\t\t\tA key path (starting, but not ending with '/')\n";

static const char *LIST_HELP_MESSAGE =
        "Usage:\n"
        "  dbdconf GVDB_PATH list DIR\n"
        "  dbdconf list GVDB_PATH DIR\n\n"
        "List the sub-keys and sub-dirs of a dir\n\n"
        "Arguments:\n"
        " GVDB_PATH\t\tA GVDB layer file path\n"
        " DIR\t\t\tA directory path (starting and ending with '/')\n";

static const char *DUMP_HELP_MESSAGE =
        "Usage:\n"
        "  dbdconf GVDB_PATH dump DIR\n"
        "  dbdconf dump GVDB_PATH DIR\n\n"
        "Dump an entire sub-path to stdout\n\n"
        "Arguments:\n"
        " GVDB_PATH\t\tA GVDB layer file path\n"
        " DIR\t\t\tA directory path (starting and ending with '/')\n";

const char *dbd_get_help_for(DbdCliInstanceCommand command) {
    switch (command) {
        default:
        case DBD_INSTANCE_COMMAND_NONE:
        case DBD_INSTANCE_COMMAND_HELP:
            return DEFAULT_HELP_MESSAGE;
        case DBD_INSTANCE_COMMAND_READ:
            return READ_HELP_MESSAGE;
        case DBD_INSTANCE_COMMAND_LIST:
            return LIST_HELP_MESSAGE;
        case DBD_INSTANCE_COMMAND_DUMP:
            return DUMP_HELP_MESSAGE;
    }
}

gboolean dbd_lexing_command(int *argc, const char ***argv, DbdCliInstance *instance) {
    if (!argc || !argv || !*argv || (**argv)[0] == '/' || (**argv)[0] == '"' || (**argv)[0] == '\'') {
        return FALSE;
    }
    if (instance->command == DBD_INSTANCE_COMMAND_HELP) {
        return FALSE;
    }

    switch ((**argv)[0]) {
        case 'h':
            // if matched any error string, then goto error sequence.
        error_sequence:
            if (instance->gvdb_file) {
                g_free((gpointer) instance->gvdb_file);
                instance->gvdb_file = NULL;
            }
            if (instance->path) {
                g_free((gpointer) instance->path);
                instance->path = NULL;
            }
            if (strcmp((**argv), "help") != 0) {
                instance->value = g_strdup_printf("%s%s", "error: dir/key must begin with a slash.\n",
                                                  dbd_get_help_for(instance->command));
            instance->command = DBD_INSTANCE_COMMAND_HELP;
                break;
            }
            else {
            --(*argc), ++(*argv);
            }
            instance->value = g_strdup(dbd_get_help_for(instance->command));
            instance->command = DBD_INSTANCE_COMMAND_HELP;
            break;
        case 'r':
            if (strcmp((**argv), "read") != 0 || instance->command != DBD_INSTANCE_COMMAND_NONE) {
                goto error_sequence;
            }
            --(*argc), ++(*argv);
            instance->command = DBD_INSTANCE_COMMAND_READ;
            break;
        case 'l':
            /// TODO: Add load, when implemented
            if (strcmp((**argv), "list") != 0 || instance->command != DBD_INSTANCE_COMMAND_NONE) {
                goto error_sequence;
            }
            --(*argc), ++(*argv);
            instance->command = DBD_INSTANCE_COMMAND_LIST;
            break;
        case 'd':
            if (strcmp((**argv), "dump") != 0 || instance->command != DBD_INSTANCE_COMMAND_NONE) {
                goto error_sequence;
            }
            --(*argc), ++(*argv);
            instance->command = DBD_INSTANCE_COMMAND_DUMP;
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

gboolean dbd_lexing_path(int *argc, const char ***argv, gchar **path, gboolean is_dir) {
    const char *lexing = **argv;
    gsize lexing_length = strlen(lexing);

    if (*lexing != '/') {
        return FALSE;
    }

    if (is_dir && lexing[lexing_length - 1] != '/') {
        return FALSE;
    }

    *path = g_strndup(lexing, lexing_length);
    --(*argc), ++(*argv);
    return TRUE;
}

DbdCliInstance *dbd_parse_args(int argc, const char **argv) {
    DbdCliInstance *instance = g_slice_new0(DbdCliInstance);
    ++argv, --argc;

    while (argc) {
        if (instance->command == DBD_INSTANCE_COMMAND_HELP) {
            break;
        }
        if ((*argv)[0] == '/' || (*argv)[0] == '.') {
            if (!instance->gvdb_file) {
                instance->gvdb_file = g_strdup(argv[0]);
                --(argc), ++(argv);
            } else if (!instance->path && (*argv)[0] != '.') {
                if (instance->command == DBD_INSTANCE_COMMAND_NONE) {
                    g_free((gpointer) instance->gvdb_file);
                    instance->gvdb_file = NULL;
                    instance->command = DBD_INSTANCE_COMMAND_HELP;
                    instance->value = g_strdup_printf("%s: %s\n%s", "error: unknown command", *argv,
                                                      DEFAULT_HELP_MESSAGE);
                    return instance;
                }
                if (instance->command == DBD_INSTANCE_COMMAND_READ) {
                    dbd_lexing_path(&argc, &argv, (gpointer) &instance->path, FALSE);
                } else if (!dbd_lexing_path(&argc, &argv, (gpointer) &instance->path, TRUE)) {
                    g_free((gpointer) instance->gvdb_file);
                    instance->gvdb_file = NULL;
                    instance->command = DBD_INSTANCE_COMMAND_HELP;
                    instance->value = g_strdup_printf("%s: %s\n%s", "error: dir must begin and end with '/'", *argv,
                                                      DEFAULT_HELP_MESSAGE);
                    return instance;
                }
            }
            else if (!dbd_lexing_command(&argc, &argv, instance)) {
                return instance;
            }
        } else if (!dbd_lexing_command(&argc, &argv, instance)) {
            return instance;
        }
    }

    return instance;
}

void dbd_free_args(DbdCliInstance *instance) {
    if (instance->gvdb_file) {
        g_free((gpointer) instance->gvdb_file);
    }
    if (instance->path) {
        g_free((gpointer) instance->path);
    }
    if (instance->value) {
        g_free((gpointer) instance->value);
    }
    g_free_sized(instance, sizeof(DbdCliInstance));
}
