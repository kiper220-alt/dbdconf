#include "private_svdb_parse.c"
#include "private_svdb_export.c"

G_DEFINE_BOXED_TYPE (SvdbTableItem, svdb_table, svdb_item_ref, svdb_item_unref)

SvdbTableItem *svdb_table_new() {
    GHashTable *table = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free,
                                              (GDestroyNotify) &svdb_item_clear_unref_dettach);
    SvdbTableItem *item = svdb_item_set_table(svdb_item_new(), table);
    g_hash_table_unref(table);
    return item;
}

SvdbTableItem *svdb_table_read_from_file(const gchar *filename, gboolean trusted, GError **error) {
    GMappedFile *mapped;
    SvdbTableItem *table;
    GBytes *bytes;

    mapped = g_mapped_file_new(filename, FALSE, error);
    if (!mapped) {
        return NULL;
    }

    bytes = g_mapped_file_get_bytes(mapped);
    table = svdb_table_read_from_bytes(bytes, trusted, error);
    g_mapped_file_unref(mapped);
    g_bytes_unref(bytes);

    g_prefix_error(error, "%s: ", filename);

    return table;
}

SvdbTableItem *svdb_table_read_from_bytes(GBytes *bytes, gboolean trusted, GError **error) {
    const struct svdb_header *header;
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

    return svdb_parse_table(data, size, byteswapped, trusted, header->root);

    invalid:
    g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "invalid gvdb header");
    return NULL;
}

gboolean svdb_table_set(SvdbTableItem *table, const gchar *key,
                              SvdbTableItem *value) {
    if (!table || table->type != SVDB_TYPE_TABLE || !key) {
        return FALSE;
    }

    if (!value) {
        return g_hash_table_remove(table->table, key);
    }
    svdb_item_set_parent(value, table);
    return g_hash_table_replace(table->table, (gpointer) g_strdup(key), svdb_item_ref(value));;
}

SvdbTableItem *svdb_table_get(SvdbTableItem *table, const gchar *key) {
    if (!table || table->type != SVDB_TYPE_TABLE) {
        return table;
    }

    SvdbTableItem *item = g_hash_table_lookup(table->table, key);
    return item;
}

GString *svdb_table_dump(SvdbTableItem *table, gchar *tablePath) {
    if (!table || table->type != SVDB_TYPE_TABLE) {
        return g_string_new(NULL);
    }
    if (tablePath == NULL) {
        tablePath = "/";
    }
    GHashTableIter iter;
    gchar *key = NULL;
    SvdbTableItem *item = NULL;
    GString *result = NULL;
    GString *sub_table = NULL;
    gboolean header_added = FALSE;

    result = g_string_new(NULL);
    sub_table = g_string_new(NULL);
    g_hash_table_iter_init(&iter, table->table);

    while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &item)) {
        if (svdb_item_get_type(item) == SVDB_TYPE_TABLE) {
            GString *tmp = g_string_new(tablePath);
            gchar *subpath;

            g_string_append_printf(tmp, "%s/", key);
            subpath = g_string_free(tmp, FALSE);

            GString *tmp2 = svdb_table_dump(item, subpath);
            if (tmp2) {
                if (tmp2->len != 0) {
                    g_string_append_len(sub_table, "\n\n", 2);
                    g_string_append_len(sub_table, tmp2->str, tmp2->len);
                }

                g_string_free(tmp2, TRUE);
            }
            continue;
        }
        if (!header_added) {
            g_string_append_printf(result, "[%s]", tablePath);
            header_added = TRUE;
        }
        GString *value_str = svdb_item_dump(item, NULL, TRUE);
        if (item->type == SVDB_TYPE_LIST) {
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

/**
 * svdb_table_list_child
 * Returns: (transfer full): value must be freed with `g_strfreev`
 */
gchar **svdb_table_list_child(SvdbTableItem *table, gsize *size) {
    gsize tmp;
    gchar **result;
    gchar **str_iter;
    SvdbTableItem* item;
    const gchar *key;

    GHashTableIter iter;

    if (!size) {
        size = &tmp;
    }
    if (!table || table->type != SVDB_TYPE_TABLE) {
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
        if (item->type == SVDB_TYPE_TABLE) {
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

gboolean svdb_table_unset(SvdbTableItem *table, const gchar *key) {
    if (!table || table->type != SVDB_TYPE_TABLE) {
        return FALSE;
    }
    return g_hash_table_remove(table->table, key);
}

SvdbTableItem *svdb_item_new() {
    SvdbTableItem *item = g_malloc0(sizeof *item);
    ++item->refcount;
    return item;
}

gboolean svdb_item_set_list(SvdbTableItem *item, const SvdbListElement *list,
                                  guint32 length) {
    if (!item) {
        return FALSE;
    }

    svdb_item_clear(item);
    item->type = SVDB_TYPE_LIST;

    if (!length || !list) {
        return TRUE;
    }

    item->length = length;
    item->list = g_malloc(length * sizeof *item->list);
    item->childs = 0;

    for (guint32 i = 0; i < length; ++i) {
        svdb_item_set_parent(list[i].item, item);
        item->list[i].item = svdb_item_ref(list[i].item);
        item->list[i].key = g_strdup(list[i].key);
    }

    return TRUE;
}

gboolean svdb_item_list_append(SvdbTableItem *item, const SvdbListElement *list,
                                     guint32 length) {
    if (!item) {
        return FALSE;
    }

    if (item->type != SVDB_TYPE_LIST) {
        svdb_item_clear(item);
        item->type = SVDB_TYPE_LIST;
    }

    if (!list || !length) {
        return TRUE;
    }

    gsize old_length = item->length;
    item->length += length;
    item->list = g_realloc(item->list, item->length * sizeof *item->list);

    for (gsize i = old_length; i < item->length; ++i) {
        svdb_item_set_parent(list[i - old_length].item, item);
        item->list[i].item = svdb_item_ref(list[i - old_length].item);
        item->list[i].key = g_strdup(list[i].key);
    }

    return TRUE;
}

gboolean svdb_item_list_append_element(SvdbTableItem *list,
                                             SvdbListElement element) {
    return svdb_item_list_append_value(list, element.key, element.item);
}

gboolean svdb_item_list_append_variant(SvdbTableItem *list, const gchar *key, GVariant *value) {
    SvdbTableItem *item = svdb_item_new();
    svdb_item_set_variant(item, value);
    gboolean result = svdb_item_list_append_value(list, key, item);

    svdb_item_unref(item);
    return result;
}

gboolean svdb_item_list_append_value(SvdbTableItem *list, const gchar *key, SvdbTableItem *value) {
    if (!list) {
        return FALSE;
    }

    if (list->type != SVDB_TYPE_LIST) {
        svdb_item_clear(list);
        list->type = SVDB_TYPE_LIST;
        list->length = 1;
        list->list = g_malloc(sizeof *list->list);
        list->list->item = svdb_item_ref(value);
        list->list->key = g_strdup(key);
        return TRUE;
    }

    for (int i = 0; i < list->length; ++i) {
        if (strcmp(list->list[i].key, key) == 0) {
            svdb_item_clear_unref_dettach(list->list[i].item);
            svdb_item_set_parent(value, list);
            list->list[i].item = svdb_item_ref(value);
            return TRUE;
        }
    }

    ++list->length;
    list->list = g_realloc(list->list, list->length * sizeof *list->list);
    svdb_item_set_parent(value, list);

    list->list[list->length - 1].key = g_strdup(key);
    list->list[list->length - 1].item = svdb_item_ref(value);
    return TRUE;
}

gboolean svdb_item_set_variant(SvdbTableItem *item, GVariant *variant) {
    if (!item) {
        return FALSE;
    }

    svdb_item_clear(item);

    if (!variant) {
        return TRUE;
    }

    item->type = SVDB_TYPE_VARIANT;
    item->variant = g_variant_ref(variant);
    return TRUE;
}

SvdbItemType svdb_item_get_type(SvdbTableItem *item) {
    if (!item) {
        return SVDB_TYPE_NONE;
    }
    return item->type;
}


const SvdbListElement *svdb_item_get_list(SvdbTableItem *item, gsize *length) {
    if (item->type != SVDB_TYPE_LIST) {
        return NULL;
    }
    if (length) {
        *length = item->length;
    }
    return item->list;
}

GHashTable *dbd_item_get_table(SvdbTableItem *item) {
    if (item->type != SVDB_TYPE_TABLE) {
        return NULL;
    }
    return g_hash_table_ref(item->table);
}

GVariant *svdb_item_get_variant(SvdbTableItem *item) {
    if (item->type != SVDB_TYPE_VARIANT) {
        return NULL;
    }
    return g_variant_ref(item->variant);
}

GString *svdb_item_dump(SvdbTableItem *item, gchar *path, gboolean valueMode) {
    if (!item) {
        return g_string_new(NULL);
    }

    switch (item->type) {
        case SVDB_TYPE_VARIANT: {
            GString *result = g_variant_print_string(item->variant, NULL, FALSE);
            if (result && valueMode) {
                for (gsize i = 0; i < result->len; ++i) {
                    if (result->str[i] == '\'') {
                        result->str[i] = '"';
                    }
                }
            }
            return result;
        }
        case SVDB_TYPE_LIST: {
            GString *str;
            if (valueMode) {
                str = g_string_new("{");
            }
            else {
                str = g_string_new("'{");
            }
            gboolean first = TRUE;
            for (int i = 0; i < item->length; ++i) {
                GString *tmp = svdb_item_dump(item->list[i].item, NULL, TRUE);
                if (first) {
                    g_string_append_printf(str, "\"%s\": %s", item->list[i].key, tmp->str);
                    first = FALSE;
                } else {
                    g_string_append_printf(str, ", \"%s\": %s", item->list[i].key, tmp->str);
                }
                g_free(tmp);
            }
            if (valueMode) {
                g_string_append_c(str, '}');
            }
            else {
                g_string_append_len(str, "}'", 2);
            }
            return str;
        }
        case SVDB_TYPE_TABLE: {
            if (!valueMode) {
                return svdb_table_dump(item, path);
            }
            GString *str = g_string_new("{");
            GHashTableIter iter;
            gchar *key;
            SvdbTableItem *value;
            g_hash_table_iter_init(&iter, item->table);

            while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &value)) {
                GString *tmp = svdb_item_dump(value, NULL, TRUE);
                g_string_append_printf(str, "\"%s\": %s", key, tmp->str);
                g_free(tmp);
            }

            g_string_append_c(str, '}');
            return str;
        }
        case SVDB_TYPE_NONE: {
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, "%s", "Corrupted item in table.");
            return g_string_new("(NULL)");
        }
    }
}

SvdbTableItem *svdb_item_ref(SvdbTableItem *item) {
    ++item->refcount;
    return item;
}

void svdb_item_unref(SvdbTableItem *item) {
    if (!--item->refcount) {
        svdb_item_clear(item);
        g_free(item);
    }
}