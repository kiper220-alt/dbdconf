#include <cli.h>
#include <svdb.h>
#include <stdio.h>

int main(int argc, const char** argv) {
    GError* error = NULL;
    SvdbTableItem* table;
    GString* output = NULL;
    DbdCliInstance *instance = dbd_parse_args(argc, argv);

    if (instance->command == DBD_INSTANCE_COMMAND_HELP) {
        printf("%s\n", instance->value);
        return 0;
    }
    if (instance->command == DBD_INSTANCE_COMMAND_NONE) {
        printf("%s", "no command present (type 'dbdconf help' for help)\n");
        return -1;
    }

    if (!g_file_test(instance->gvdb_file, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) {
        printf("%s %s %s", "file ", instance->gvdb_file, " not found\n");
        return -2;
    }

    table = svdb_table_read_from_file(instance->gvdb_file, FALSE, &error);

    if (error || !table) {
        printf("%s %s %s", "error while reading ", instance->gvdb_file, "\n");
        if (error) {
            g_log (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "%s", error->message);
        }
        return -2;
    }

    switch (instance->command) {
        case DBD_INSTANCE_COMMAND_DUMP: {
            output = svdb_dump_path(table, instance->path, &error);
            break;
        }
        case DBD_INSTANCE_COMMAND_LIST: {
            output = svdb_list_path(table, instance->path, &error);
            break;
        }
        case DBD_INSTANCE_COMMAND_READ: {
            output = svdb_read_path(table, instance->path, &error);
            break;
        }
    }

    if (error) {
        g_log (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "%s", error->message);
        return -3;
    }
    if (output) {
        printf("%s\n", output->str);
        g_string_free(output, TRUE);
    }

    svdb_item_unref(table);
    dbd_free_args(instance);
    return 0;
}
