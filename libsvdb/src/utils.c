#include <svdb.h>

static const gchar* ERROR_QUARK_STRING = "DBD_TABLE_JOIN";

SvdbTableItem* svdb_table_join_to(SvdbTableItem* table, const gchar* path, gboolean is_dir, GError** error) {
    if (!path || !*path) {
        g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "Path is empty path");
        return NULL;
    }
    if (path[0] != '/') {
        g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "The path must start with '/'");
    }
    if (path[1] == '\0' || !table) {
        return table;
    }

    do {
        const gchar *begin = ++path;

        while(*path && *path != '/') {
            ++path;
        }

        if (is_dir && !*path) {
            if (path == begin) {
                return table;
            }
            g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "The path must end with '/'");
            return NULL;
        }
        if (!is_dir && *path == '/' && !path[1]){
            g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "The path must not end with '/'");
            return NULL;
        }

        gchar* key = g_strndup(begin, path - begin);
        table = svdb_table_get(table, key);
        g_free(key);
    }
    while (*path && table && svdb_item_get_type(table) == SVDB_TYPE_TABLE);

    if (is_dir && table && svdb_item_get_type(table) == SVDB_TYPE_TABLE) {
        return table;
    }
    if (!is_dir && table && svdb_item_get_type(table) != SVDB_TYPE_TABLE) {
        return table;
    }
    return NULL;
}

GString* svdb_dump_path(SvdbTableItem* table, const gchar* path, GError** error) {
    table = svdb_table_join_to(table, path, TRUE, error);
    if (!table) {
        return NULL;
    }
    return svdb_table_dump(table, "/");
}
GString* svdb_list_path(SvdbTableItem* table, const gchar* path, GError** error) {
    table = svdb_table_join_to(table, path, TRUE, error);
    GString *result;
    gsize size;
    gchar** keys;

    if (!table) {
        return NULL;
    }

    keys = svdb_table_list_child(table, &size);

    if (!size) {
        g_strfreev(keys);
        return g_string_new("");
    }

    result = g_string_new(keys[0]);
    for (gsize i = 1; i < size; ++i) {
        g_string_append_printf(result, "\n%s", keys[i]);
    }

    g_strfreev(keys);
    return result;
}
GString* svdb_read_path(SvdbTableItem* table, const gchar* path, GError** error) {
    table = svdb_table_join_to(table, path, FALSE, error);
    if (!table) {
        return NULL;
    }

    return svdb_item_dump(table, "/", FALSE);
}
