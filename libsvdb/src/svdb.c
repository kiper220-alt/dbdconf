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

SvdbTableItem *svdb_table_get(const SvdbTableItem *table, const gchar *key) {
    if (!table || table->type != SVDB_TYPE_TABLE) {
        return NULL;
    }

    SvdbTableItem *item = g_hash_table_lookup(table->table, key);
    return svdb_item_ref(item);
}

/**
 * svdb_table_list_child
 * Returns: (transfer full): value must be freed with `g_strfreev`
 */
gchar **svdb_table_list_child(const SvdbTableItem *table, gsize *size) {
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

gboolean svdb_item_list_remove_element(SvdbTableItem *item, const gchar *element) {
    if (!item || !element || item->type != SVDB_TYPE_LIST || !item->length) {
        return FALSE;
    }
    gsize pos = -1;

    for (gsize i = 0; i < item->length; ++i) {
        if (strcmp(item->list[i].key, element) == 0) {
            pos = i;
            break;
        }
    }

    if (pos == -1) {
        return FALSE;
    }

    svdb_item_clear_unref_dettach(item->list[pos].item);
    g_free(item->list[pos].key);

    for (gsize i = pos; i < item->length - 1; ++i) {
        item->list[i].item = item->list[i + 1].item;
    }
    return TRUE;
}

gboolean svdb_item_list_remove_elements(SvdbTableItem *item, const gchar **elements, gsize nelements, gboolean exist_cancel) {
    if (!item || !elements || item->type != SVDB_TYPE_LIST) {
        return FALSE;
    }

    if (!*elements) {
        return TRUE;
    }

    gsize size = 0;
    gboolean* exists = NULL;
    SvdbListElement* new_list = NULL;

    if (exist_cancel) {
        exists = g_new0(gboolean, nelements);
    }

    for (gsize i = 0; i < item->length; ++i) {
        gboolean founded = FALSE;
        for (gsize j = 0; j < nelements; ++j) {
            if (strcmp(item->list[i].key, elements[j]) == 0) {
                founded = TRUE;
                if (exist_cancel) {
                    exists[j] = TRUE;
                }
                break;
            }
        }
        if (!founded) {
            new_list = g_realloc_n(new_list, sizeof *new_list, ++size);
            new_list[size - 1] = item->list[i];
            item->list[i].item = NULL;
            item->list[i].key = NULL;
        }
    }

    if (exist_cancel) {
        gboolean all = TRUE;
        for (gsize i = 0; i < nelements; ++i) {
            if (!exists[i]) {
                all = FALSE;
                break;
            }
        }
        if (!all) {
            gsize alt_count = 0;
            for (gsize i = 0; i < item->length; ++i) {
                if (!item->list[i].key) {
                    item->list[i] = new_list[alt_count++];
                }
            }
            g_free(new_list);
            g_free(exists);
            return FALSE;
        }
    }
    for (gsize i = item->length; i > 0; --i) {
        if (!item->list[i - 1].key) {
            g_free(item->list[i - 1].key);
            svdb_item_clear_unref_dettach(item->list[i - 1].item);
        }
    }
    g_free(item->list);
    item->list = new_list;
    item->length = size;
    return TRUE;
}

gboolean svdb_item_list_clear(SvdbTableItem *item) {
    if (!item || item->type != SVDB_TYPE_LIST) {
        return FALSE;
    }
    for (gsize i = item->length; i > 0; --i) {
        g_free(item->list[i - 1].key);
        svdb_item_clear_unref_dettach(item->list[i - 1].item);
    }
    g_free(item->list);
    item->list = NULL;
    item->length = 0;
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

SvdbItemType svdb_item_get_type(const SvdbTableItem *item) {
    if (!item) {
        return SVDB_TYPE_NONE;
    }
    return item->type;
}


const SvdbListElement *svdb_item_get_list(const SvdbTableItem *item, gsize *length) {
    if (!item || item->type != SVDB_TYPE_LIST) {
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

GVariant *svdb_item_get_variant(const SvdbTableItem *item) {
    if (item->type != SVDB_TYPE_VARIANT) {
        return NULL;
    }
    return g_variant_ref(item->variant);
}

GString *svdb_item_dump(const SvdbTableItem *item, const gchar *path, gboolean valueMode) {
    if (!item) {
        return g_string_new(NULL);
    }

    if (!path) {
        path = "/";
    }

    switch (item->type) {
        case SVDB_TYPE_LIST: {
            GString *result = NULL;
            GString *tmp;

            if (valueMode) {
                if (item->length == 0) {
                    return g_string_new_len("{}", 2);
                }
                result = g_string_new("{");

                tmp = svdb_item_dump(item->list[0].item, path, TRUE);
                g_string_append_printf(result, "%s: %s", item->list[0].key, tmp->str);
                g_string_free(tmp, TRUE);

                for (int i = 1; i < item->length; ++i) {
                    tmp = svdb_item_dump(item->list[i].item, path, TRUE);
                    g_string_append_printf(result, ", %s: %s", item->list[i].key, tmp->str);
                    g_string_free(tmp, TRUE);
                }

                g_string_append_c(result, '}');

                return result;
            }

            if (item->length == 0) {
                return g_string_new(NULL);
            }

            GString *other = NULL;
            for (int i = 0; i < item->length; ++i) {
                if (item->list[i].item->type == SVDB_TYPE_LIST) {
                    gchar *sub_path = g_strdup_printf("%s%s", path, item->list[i].key);
                    tmp = svdb_item_dump(item->list[i].item, sub_path, FALSE);

                    if (tmp) {
                        if (!other) {
                            other = g_string_new(NULL);
                        }
                        else {
                            g_string_append_len(other, "\n\n", 2);
                        }
                        g_string_append_len(other, tmp->str, tmp->len);

                        g_string_free(tmp, TRUE);
                    }

                    g_free(sub_path);
                    continue;
                }

                if (!result) {
                    result = g_string_new_len("[", 1);
                    g_string_append(result, path);
                    g_string_append_c(result, ']');
                }
                tmp = svdb_item_dump(item->list[i].item, path, FALSE);
                g_string_append_printf(result, "\n%s=%s", item->list[i].key, tmp->str);
                g_string_free(tmp, TRUE);
            }
            if (other) {
                if (!result) {
                    result = other;
                }
                else {
                    g_string_append_len(result, "\n\n", 2);
                    g_string_append_len(result, other->str, other->len);
                    g_string_free(other, TRUE);
                }
            }

            return result;
        }
        case SVDB_TYPE_VARIANT: {
            return g_variant_print_string(item->variant, NULL, FALSE);
        }

        case SVDB_TYPE_TABLE: {
            const gchar* key;
            const SvdbTableItem *key_item;
            GHashTableIter iter;
            GString *result;
            GString *tmp;

            g_hash_table_iter_init(&iter, item->table);

            if (!g_hash_table_iter_next(&iter, (gpointer) &key, (gpointer) &key_item)) {
                return g_string_new("{}");
            }

            result = g_string_new("{");

            tmp = svdb_item_dump(key_item, path, TRUE);
            g_string_append_printf(result, "%s: %s", key, tmp->str);
            g_string_free(tmp, TRUE);

            while (g_hash_table_iter_next(&iter, (gpointer) &key, (gpointer) &key_item)) {
                tmp = svdb_item_dump(key_item, path, TRUE);
                g_string_append_printf(result, ", %s: %s", key, tmp->str);
                g_string_free(tmp, TRUE);
            }

            g_string_append_c(result, '}');

            return result;
        }
    }
}

SvdbTableItem *svdb_item_list_get_element(const SvdbTableItem *list, const gchar* key) {
    if (!list || list->type != SVDB_TYPE_LIST || list->length == 0 || !key) {
        return NULL;
    }

    // Temporary linear search
    for (int i = 0; i < list->length; ++i) {
        if (strcmp(list->list[i].key, key) == 0) {
            return svdb_item_ref(list->list[i].item);
        }
    }

    return NULL;
}

SvdbTableItem *svdb_item_ref(const SvdbTableItem *item) {
    if (!item) {
        return NULL;
    }
    ++((SvdbTableItem*)item)->refcount;
    return (gpointer) item;
}

void svdb_item_unref(SvdbTableItem *item) {
    if (!item) {
        return;
    }
    if (!--item->refcount) {
        svdb_item_clear(item);
        g_free(item);
    }
}