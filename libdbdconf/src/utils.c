#include <libdbdconf/utils.h>

GVariantTableItem* dbd_table_join_to(GVariantTableItem* table, const gchar* path, gboolean is_dir, GError** error) {
    if (!path || !*path) {
        g_error_new_literal(0, 0, "Path is empty path");
        return NULL;
    }
    if (path[0] != '/') {
        g_error_new_literal(0, 0, "The path must start with '/'");
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
            g_error_new_literal(0, 0, "The path must end with '/'");
            return NULL;
        }
        if (!is_dir && *path == '/' && !path[1]){
            g_error_new_literal(0, 0, "The path must not end with '/'");
            return NULL;
        }

        gchar* key = g_strndup(begin, path - begin);
        table = dbd_table_get(table, key);
        g_free(key);
    }
    while (*path && table && dbd_item_get_type(table) == DBD_TYPE_TABLE);

    if (is_dir && table && dbd_item_get_type(table) == DBD_TYPE_TABLE) {
        return table;
    }
    if (!is_dir && table && dbd_item_get_type(table) != DBD_TYPE_TABLE) {
        return table;
    }
    return NULL;
}

GString* dbd_dump_path(GVariantTableItem* table, const gchar* path, GError** error) {
    table = dbd_table_join_to(table, path, TRUE, error);
    if (!table) {
        return NULL;
    }
    return dbd_table_dump(table, "/");
}
GString* dbd_list_path(GVariantTableItem* table, const gchar* path, GError** error) {
    table = dbd_table_join_to(table, path, TRUE, error);
    GString *result;
    gsize size;
    gchar** keys;

    if (!table) {
        return NULL;
    }

    keys = dbd_table_list_child(table, &size);

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
GString* dbd_read_path(GVariantTableItem* table, const gchar* path, GError** error) {
    table = dbd_table_join_to(table, path, FALSE, error);
    if (!table) {
        return NULL;
    }

    return dbd_item_dump(table, "/", FALSE);
}