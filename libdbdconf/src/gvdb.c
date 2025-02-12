#include "private_gvdb_parse.h"
#include "private_gvdb_export.h"

GVariantTableItem *dbd_table_new() {
    GHashTable *table = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free,
                                              (GDestroyNotify) &dbd_item_clear_unref_dettach);
    GVariantTableItem *item = dbd_item_set_table(dbd_item_new(), table);
    g_hash_table_unref(table);
    return item;
}

GVariantTableItem *dbd_table_read_from_file(const gchar *filename, gboolean trusted, GError **error) {
    GMappedFile *mapped;
    GVariantTableItem *table;
    GBytes *bytes;

    mapped = g_mapped_file_new(filename, FALSE, error);
    if (!mapped) {
        return NULL;
    }

    bytes = g_mapped_file_get_bytes(mapped);
    table = dbd_table_read_from_bytes(bytes, trusted, error);
    g_mapped_file_unref(mapped);
    g_bytes_unref(bytes);

    g_prefix_error(error, "%s: ", filename);

    return table;
}

GVariantTableItem *dbd_table_read_from_bytes(GBytes *bytes, gboolean trusted, GError **error) {
    const struct gvdb_header *header;
    gsize size;
    gboolean byteswapped;
    gconstpointer data;

    data = g_bytes_get_data(bytes, &size);

    if (size < sizeof *header) {
        goto invalid;
    }

    header = (gpointer) data;

    if (header->signature[0] == GVDB_SIGNATURE0 && header->signature[1] == GVDB_SIGNATURE1
        && guint32_from_le(header->version) == 0) {
        byteswapped = FALSE;
    } else if (header->signature[0] == GVDB_SWAPPED_SIGNATURE0
               && header->signature[1] == GVDB_SWAPPED_SIGNATURE1
               && guint32_from_le(header->version) == 0) {

        byteswapped = TRUE;
    } else {
        goto invalid;
    }

    return dbd_parse_table(data, size, byteswapped, trusted, header->root);

    invalid:
    g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "invalid gvdb header");
    return NULL;
}

GVariantTableItem *dbd_table_set(GVariantTableItem *table, const gchar *key,
                                 GVariantTableItem *value) {
    if (!table || table->type != DBD_TYPE_TABLE || !key) {
        return table;
    }

    if (!value) {
        g_hash_table_remove(table->table, key);
        return table;
    }
    dbd_item_set_parent(value, table);
    g_hash_table_replace(table->table, (gpointer) g_strdup(key), dbd_item_ref(value));
    return table;
}

GVariantTableItem *dbd_table_get(GVariantTableItem *table, const gchar *key) {
    if (!table || table->type != DBD_TYPE_TABLE) {
        return table;
    }

    GVariantTableItem *item = g_hash_table_lookup(table->table, key);
    return item;
}

GString *dbd_table_dump(GVariantTableItem *table, gchar *tablePath) {
    if (!table || table->type != DBD_TYPE_TABLE) {
        return g_string_new(NULL);
    }
    if (tablePath == NULL) {
        tablePath = "/";
    }
    GHashTableIter iter;
    gchar *key = NULL;
    GVariantTableItem *item = NULL;
    GString *result = NULL;
    GString *sub_table = NULL;
    gboolean header_added = FALSE;

    result = g_string_new(NULL);
    sub_table = g_string_new(NULL);
    g_hash_table_iter_init(&iter, table->table);

    while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &item)) {
        if (dbd_item_get_type(item) == DBD_TYPE_TABLE) {
            GString *tmp = g_string_new(tablePath);
            gchar *subpath;

            g_string_append_printf(tmp, "%s/", key);
            subpath = g_string_free(tmp, FALSE);

            GString *tmp2 = dbd_table_dump(item, subpath);
            g_string_append_len(sub_table, "\n\n", 2);
            g_string_append_len(sub_table, tmp2->str, tmp2->len);
            g_string_free(tmp2, TRUE);
            continue;
        }
        if (!header_added) {
            g_string_append_printf(result, "[%s]", tablePath);
            header_added = TRUE;
        }
        GString *value_str = dbd_item_dump(item, NULL, TRUE);
        if (item->type == DBD_TYPE_LIST) {
            g_string_append_printf(result, "\n%s='%s'", key, value_str->str);
        } else {
            g_string_append_printf(result, "\n%s=%s", key, value_str->str);
        }
        g_string_free(value_str, TRUE);
    }
    if (sub_table->len != 0) {
        g_string_append_len(result, sub_table->str, sub_table->len);
    }
    g_string_free(sub_table, TRUE);

    return result;
}

gchar **dbd_table_list_child(GVariantTableItem *table, gsize *size) {
    gsize tmp;
    gchar **result;
    gchar **str_iter;
    GVariantTableItem* item;
    const gchar *key;

    GHashTableIter iter;

    if (!size) {
        size = &tmp;
    }
    if (!table || table->type != DBD_TYPE_TABLE) {
        *size = 0;
        return NULL;
    }

    *size = g_hash_table_size(table->table);

    if (!*size) {
        return NULL;
    }

    result = g_new(gchar*, *size + 1);
    str_iter = result;

    g_hash_table_iter_init(&iter, table->table);
    while (g_hash_table_iter_next(&iter, (gpointer) &key, (gpointer) &item)) {
        if (item->type == DBD_TYPE_TABLE) {
            *str_iter = g_strdup_printf("%s/", key);
        } 
        else {
            *str_iter = g_strdup(key);
        }
        ++str_iter;
    }
    *str_iter = NULL;

    // result real length must be equal `g_hash_table_size()`.
    g_assert(str_iter - result == *size);

    return result;
}

GVariantTableItem *dbd_table_unset(GVariantTableItem *table, const gchar *key) {
    if (!table || table->type != DBD_TYPE_TABLE) {
        return table;
    }
    g_hash_table_remove(table->table, key);
}

GVariantTableItem *dbd_item_new() {
    GVariantTableItem *item = g_malloc0(sizeof *item);
    ++item->refcount;
    return item;
}

GVariantTableItem *dbd_item_set_list(GVariantTableItem *item, GVariantListElement *list,
                                     guint32 length) {
    if (!item) {
        return NULL;
    }

    dbd_item_clear(item);
    item->type = DBD_TYPE_LIST;

    if (!length || !list) {
        return item;
    }

    item->length = length;
    item->list = g_malloc(length * sizeof *item->list);
    item->childs = 0;

    for (guint32 i = 0; i < length; ++i) {
        dbd_item_set_parent(list[i].item, item);
        item->list[i].item = dbd_item_ref(list[i].item);
        item->list[i].key = g_strdup(list[i].key);
    }

    return item;
}

GVariantTableItem *dbd_item_list_append(GVariantTableItem *item, GVariantListElement *list,
                                        guint32 length) {
    if (!item) {
        return NULL;
    }

    if (item->type != DBD_TYPE_LIST) {
        dbd_item_clear(item);
        item->type = DBD_TYPE_LIST;
    }

    if (!list || !length) {
        return NULL;
    }

    int old_length = item->length;
    item->length += length;
    item->list = g_realloc(item->list, item->length * sizeof *item->list);

    for (guint32 i = old_length; i < item->length; ++i) {
        dbd_item_set_parent(list[i - old_length].item, item);
        item->list[i].item = dbd_item_ref(list[i - old_length].item);
        item->list[i].key = g_strdup(list[i].key);
    }

    return item;
}

GVariantTableItem *dbd_item_list_append_element(GVariantTableItem *list,
                                                GVariantListElement element) {
    return dbd_item_list_append_value(list, element.key, element.item);
}

GVariantTableItem *dbd_item_list_append_variant(GVariantTableItem *list, const gchar *key, GVariant *value) {
    GVariantTableItem *item = dbd_item_set_variant(dbd_item_new(), value);
    GVariantTableItem *result = dbd_item_list_append_value(list, key, item);

    dbd_item_unref(item);
    return result;
}

GVariantTableItem *dbd_item_list_append_value(GVariantTableItem *list, const gchar *key, GVariantTableItem *value) {
    if (!list) {
        return NULL;
    }

    if (list->type != DBD_TYPE_LIST) {
        dbd_item_clear(list);
        list->type = DBD_TYPE_LIST;
        list->length = 1;
        list->list = g_malloc(sizeof *list->list);
        list->list->item = dbd_item_ref(value);
        list->list->key = g_strdup(key);
        return list;
    }

    for (int i = 0; i < list->length; ++i) {
        if (strcmp(list->list[i].key, key) == 0) {
            dbd_item_clear_unref_dettach(list->list[i].item);
            dbd_item_set_parent(value, list);
            list->list[i].item = dbd_item_ref(value);
            return list;
        }
    }

    ++list->length;
    list->list = g_realloc(list->list, list->length * sizeof *list->list);
    dbd_item_set_parent(value, list);

    list->list[list->length - 1].key = g_strdup(key);
    list->list[list->length - 1].item = dbd_item_ref(value);
    return list;
}

GVariantTableItem *dbd_item_set_variant(GVariantTableItem *item, GVariant *variant) {
    if (!item) {
        return NULL;
    }

    dbd_item_clear(item);

    if (!variant) {
        return NULL;
    }

    item->type = DBD_TYPE_VARIANT;
    item->variant = g_variant_ref(variant);
    return item;
}

GVariantTableItemType dbd_item_get_type(GVariantTableItem *item) {
    if (!item) {
        return DBD_TYPE_NONE;
    }
    return item->type;
}

// TODO: make rc
GVariantListElement *dbd_item_get_list(GVariantTableItem *item, gsize *length) {
    if (item->type != DBD_TYPE_LIST) {
        return NULL;
    }
    if (length) {
        *length = item->length;
    }
    return item->list;
}

GHashTable *dbd_item_get_table(GVariantTableItem *item) {
    if (item->type != DBD_TYPE_TABLE) {
        return NULL;
    }
    return g_hash_table_ref(item->table);
}

GVariant *dbd_item_get_variant(GVariantTableItem *item) {
    if (item->type != DBD_TYPE_VARIANT) {
        return NULL;
    }
    return g_variant_ref(item->variant);
}

GString *dbd_item_dump(GVariantTableItem *item, gchar *path, gboolean listMode) {
    if (!item) {
        return g_string_new(NULL);
    }

    switch (item->type) {
        case DBD_TYPE_VARIANT: {
            GString *result = g_variant_print_string(item->variant, NULL, FALSE);
            if (listMode) {
                for (gsize i = 0; i < result->len; ++i) {
                    if (result->str[i] == '\'') {
                        result->str[i] = '"';
                    }
                }
            }
            return result;
        }
        case DBD_TYPE_LIST: {
            GString *str;
            if (listMode) {
                str = g_string_new("{");
            }
            else {
                str = g_string_new("'{");
            }
            gboolean first = TRUE;
            for (int i = 0; i < item->length; ++i) {
                GString *tmp = dbd_item_dump(item->list[i].item, NULL, TRUE);
                if (first) {
                    g_string_append_printf(str, "\"%s\": %s", item->list[i].key, tmp->str);
                    first = FALSE;
                } else {
                    g_string_append_printf(str, ", \"%s\": %s", item->list[i].key, tmp->str);
                }
                g_free(tmp);
            }
            if (listMode) {
                g_string_append_c(str, '}');
            }
            else {
                g_string_append_len(str, "}'", 2);
            }
            return str;
        }
        case DBD_TYPE_TABLE: {
            if (!listMode) {
                return dbd_table_dump(item, path);
            }
            GString *str = g_string_new("{");
            GHashTableIter iter;
            gchar *key;
            GVariantTableItem *value;
            g_hash_table_iter_init(&iter, item->table);

            while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &value)) {
                GString *tmp = dbd_item_dump(value, NULL, TRUE);
                g_string_append_printf(str, "\"%s\": %s", key, tmp->str);
                g_free(tmp);
            }

            g_string_append_c(str, '}');
            return str;
        }
        case DBD_TYPE_NONE: {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "%s", "Corrupted item in table.");
            return g_string_new("(NULL)");
        }
    }
}

GVariantTableItem *dbd_item_ref(GVariantTableItem *item) {
    ++item->refcount;
    return item;
}

void dbd_item_unref(GVariantTableItem *item) {
    if (!--item->refcount) {
        dbd_item_clear(item);
        g_free(item);
    }
}

void test_dbd_table_empty() {
    GVariantTableItem *table = dbd_table_new();
    dbd_item_unref(table);
}

void test_dbd_table_set() {
    GVariantTableItem *table = NULL;
    GVariantTableItem *item = dbd_item_new();
    GVariant *variant = g_variant_new_string("test");

    dbd_item_set_variant(item, variant);
    g_variant_unref(variant);

    g_assert(dbd_table_set(table, NULL, NULL) == NULL);

    table = dbd_table_new();

    g_assert(dbd_table_set(table, NULL, NULL) == table);
    g_assert(table->childs == 0);
    g_assert(g_hash_table_size(table->table) == 0);
    g_assert(table->refcount == 1);

    g_assert(dbd_table_set(table, "NULL", NULL) == table);
    g_assert(table->childs == 0);
    g_assert(g_hash_table_size(table->table) == 0);
    g_assert(table->refcount == 1);

    g_assert(dbd_table_set(table, "NULL", item) == table);
    g_assert(table->childs == 1);
    g_assert(g_hash_table_size(table->table) == 1);
    g_assert(table->refcount == 1);
    g_assert(item->parent == table);
    g_assert(item->refcount == 2);

    g_assert(dbd_table_set(table, "NULL", NULL) == table);
    g_assert(table->childs == 0);
    g_assert(g_hash_table_size(table->table) == 0);
    g_assert(table->refcount == 1);
    g_assert(item->parent == NULL);
    g_assert(item->refcount == 1);

    dbd_item_unref(table);
    dbd_item_unref(item);
}

void test_dbd_hash() {
    const gchar *key = "";
    guint32 str_len = 1234567890;

    g_assert(dbd_hash(key, NULL) == 5381);
    dbd_hash(key, &str_len);
    g_assert(str_len == 0);

    key = "123";

    g_assert(dbd_hash(key, &str_len) != 5381);
    g_assert(str_len == 3);

    g_assert(dbd_hash("1", NULL) != dbd_hash("2", NULL));
    g_assert(dbd_hash("2", NULL) != dbd_hash("3", NULL));
    g_assert(dbd_hash("3", NULL) != dbd_hash("4", NULL));
}

void test_dbd_item_type_to_from_char() {
    g_assert(dbd_item_type_to_char(DBD_TYPE_TABLE) == 'H');
    g_assert(dbd_item_type_to_char(DBD_TYPE_LIST) == 'L');
    g_assert(dbd_item_type_to_char(DBD_TYPE_VARIANT) == 'v');
    g_assert(dbd_item_type_to_char(DBD_TYPE_NONE) == 0);

    g_assert(dbd_item_type_to_char(dbd_item_char_to_type('H')) == 'H');
    g_assert(dbd_item_type_to_char(dbd_item_char_to_type('L')) == 'L');
    g_assert(dbd_item_type_to_char(dbd_item_char_to_type('v')) == 'v');
    g_assert(dbd_item_type_to_char(dbd_item_char_to_type('a')) == 0);

    g_assert(dbd_item_char_to_type(dbd_item_type_to_char(DBD_TYPE_TABLE)) == DBD_TYPE_TABLE);
    g_assert(dbd_item_char_to_type(dbd_item_type_to_char(DBD_TYPE_LIST)) == DBD_TYPE_LIST);
    g_assert(dbd_item_char_to_type(dbd_item_type_to_char(DBD_TYPE_VARIANT)) == DBD_TYPE_VARIANT);
    g_assert(dbd_item_char_to_type(dbd_item_type_to_char(DBD_TYPE_NONE)) == DBD_TYPE_NONE);
}

void run_test() {
    test_dbd_table_empty();
    test_dbd_table_set();
    test_dbd_hash();
    test_dbd_item_type_to_from_char();
}
