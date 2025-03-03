#include <svdb.h>

static const gchar* ERROR_QUARK_STRING = "DBD_TABLE_JOIN";

SvdbTableItem* svdb_table_join_to(const SvdbTableItem* table, const gchar* path, gboolean is_dir, GError** error) {
    if (!path || !*path) {
        g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "No path is present is empty");
        return NULL;
    }
    if (*path != '/') {
        g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "The path must start with '/'");
    }
    ++path;
    SvdbTableItem* result = svdb_table_get(table, "/");
    SvdbTableItem* tmp = NULL;

    if (svdb_item_get_type(result) != SVDB_TYPE_LIST) {
        return 0;
    }

    while (*path) {
        const char* begin = path;
        char backup;
        while (*path && *path != '/') {
            ++path;
        }

        char* key;

        if (!*path) {
            if (is_dir) {
                svdb_item_unref(result);
                g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "The dir must end with '/'");
            }
            key = strndup(begin, path - begin);
            tmp = svdb_item_list_get_element(result, key);
            svdb_item_unref(result);

            if (svdb_item_get_type(tmp) == SVDB_TYPE_LIST) {
                svdb_item_unref(tmp);
                return NULL;
            }

            return tmp;
        }
        ++path;
        key = strndup(begin, path - begin);

        tmp = svdb_item_list_get_element(result, key);
        svdb_item_unref(result);
        result = tmp;

        g_free(key);

        if (svdb_item_get_type(result) != SVDB_TYPE_LIST) {
            svdb_item_unref(result);
            return NULL;
        }
    }

    if (!is_dir) {
        svdb_item_unref(result);
        g_error_new_literal(g_quark_from_static_string(ERROR_QUARK_STRING), 0, "Key can't ended with '/'");
    }

    return result;
}

GString* svdb_dump_path(const SvdbTableItem* table, const gchar* path, GError** error) {
    table = svdb_table_join_to(table, path, TRUE, error);
    if (!table) {
        return NULL;
    }
    GString* result = svdb_item_dump(table, "/", FALSE);
    svdb_item_unref((gpointer) table); // isn't const, see svdb_table_join_to signature.
    return result;
}

GString* svdb_list_path(const SvdbTableItem* table, const gchar* path, GError** error) {
    if (!table || !path) {
        return NULL;
    }
    table = svdb_table_join_to(table, path, TRUE, error);

    if (!table) {
        return NULL;
    }

    GString *result = NULL;
    gsize size;

    const SvdbListElement* elements = svdb_item_get_list(table, &size);

    if (!elements) {
        return NULL;
    }

    for (gsize i = 0; i < size; ++i) {
        if (!result) {
            result = g_string_new(elements[i].key);
            continue;
        }
        g_string_append_printf(result, "\n%s", elements[i].key);
    }

    return result;
}
GString* svdb_read_path(const SvdbTableItem* table, const gchar* path, GError** error) {
    table = svdb_table_join_to(table, path, FALSE, error);
    if (!table || svdb_item_get_type(table) == SVDB_TYPE_LIST) {
        return NULL;
    }

    return svdb_item_dump(table, "/", FALSE);
}
