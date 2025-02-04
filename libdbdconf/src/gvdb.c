#include "private_gvdb_parse.h"
#include "private_gvdb_export.h"

GVariantTableItem *dbd_table_new()
{
    GHashTable *table = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free,
                                              (GDestroyNotify)&dbd_item_clear_unref_dettach);
    GVariantTableItem *item = dbd_item_set_table(dbd_item_new(), table);
    g_hash_table_unref(table);
    return item;
}

GVariantTableItem *dbd_table_read_from_file(const gchar *filename, gboolean trusted, GError **error)
{
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
GVariantTableItem *dbd_table_read_from_bytes(GBytes *bytes, gboolean trusted, GError **error)
{
    const struct gvdb_header *header;
    gsize size;
    gboolean byteswapped;
    gconstpointer data;

    data = g_bytes_get_data(bytes, &size);

    if (size < sizeof *header) {
        goto invalid;
    }

    header = (gpointer)data;

    if (header->signature[0] == GVDB_SIGNATURE0 && header->signature[1] == GVDB_SIGNATURE1
        && guint32_from_le(header->version) == 0) {
        byteswapped = FALSE;
    }

    else if (header->signature[0] == GVDB_SWAPPED_SIGNATURE0
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
                                 GVariantTableItem *value)
{
    if (!table || table->type != DBD_TYPE_TABLE) {
        return table;
    }

    if (!value) {
        g_hash_table_remove(table->table, key);
        return table;
    }
    dbd_item_set_parent(value, table);
    g_hash_table_replace(table->table, key, dbd_item_ref(value));
    return table;
}
GVariantTableItem *dbd_table_get(GVariantTableItem *table, const gchar *key)
{
    if (!table || table->type != DBD_TYPE_TABLE) {
        return table;
    }

    GVariantTableItem *item = g_hash_table_lookup(table->table, key);
    if (!item || item->type != DBD_TYPE_TABLE) {
        return NULL;
    }
    return item;
}

GString *dbd_table_dump(GVariantTableItem *table, gchar *tablePath)
{
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

    while (g_hash_table_iter_next(&iter, &key, &item)) {
        if (dbd_item_get_type(item) == DBD_TYPE_TABLE) {
            GString *tmp = g_string_new(tablePath);
            gchar *subpath;

            g_string_append_printf(tmp, "%s/", key);
            subpath = g_string_free(tmp, FALSE);

            GString *tmp2 = dbd_table_dump(item, subpath);
            g_string_append_len(sub_table, tmp2->str, tmp2->len);
            g_string_free(tmp2, TRUE);
            continue;
        }
        if (!header_added) {
            g_string_append_printf(result, "[%s]", tablePath);
            header_added = TRUE;
        }
        GString *value_str = dbd_item_dump(item, NULL, TRUE);
        g_string_append_printf(result, "\n%s:%s", key, value_str->str);
        g_free(value_str);
    }
    if (sub_table->len != 0) {
        g_string_append_len(result, "\n\n", 2);
        g_string_append_len(result, sub_table->str, sub_table->len);
    }
    g_string_free(sub_table, TRUE);

    return result;
}

GVariantTableItem *dbd_table_unset(GVariantTableItem *table, const gchar *key)
{
    if (!table || table->type != DBD_TYPE_TABLE) {
        return table;
    }
    g_hash_table_remove(table->table, key);
}

GVariantTableItem *dbd_item_new()
{
    GVariantTableItem *item = g_malloc0(sizeof *item);
    ++item->refcount;
    return item;
}

GVariantTableItem *dbd_item_set_list(GVariantTableItem *item, GVariantListElement *list,
                                     guint32 length)
{
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
                                        guint32 length)
{
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

GVariantTableItem *dbd_item_list_append_element(GVariantTableItem *item,
                                                GVariantListElement element)
{
    if (!item) {
        return NULL;
    }

    if (item->type != DBD_TYPE_LIST) {
        dbd_item_clear(item);
        item->length = 1;
        item->list = g_malloc(sizeof *item->list);
        item->list->item = dbd_item_ref(element.item);
        item->list->key = g_strdup(element.key);
        return item;
    }

    ++item->length;
    item->list = g_realloc(item->list, item->length * sizeof *item->list);

    dbd_item_set_parent(element.item, item);
    item->list[item->length - 1].item = dbd_item_ref(element.item);
    item->list[item->length - 1].key = g_strdup(element.key);
    return item;
}

GVariantTableItem *dbd_item_set_variant(GVariantTableItem *item, GVariant *variant)
{
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

GVariantTableItemType dbd_item_get_type(GVariantTableItem *item)
{
    return item->type;
}

// TODO: make rc
GVariantListElement *dbd_item_get_list(GVariantTableItem *item, gsize *length)
{
    if (item->type != DBD_TYPE_LIST) {
        return NULL;
    }
    if (length) {
        *length = item->length;
    }
    return item->list;
}
GHashTable *dbd_item_get_table(GVariantTableItem *item)
{
    if (item->type != DBD_TYPE_TABLE) {
        return NULL;
    }
    return g_hash_table_ref(item->table);
}
GVariant *dbd_item_get_variant(GVariantTableItem *item)
{
    if (item->type != DBD_TYPE_VARIANT) {
        return NULL;
    }
    return g_variant_ref(item->variant);
}
GString *dbd_item_dump(GVariantTableItem *item, gchar *path, gboolean listMode)
{
    if (!item) {
        return g_string_new(NULL);
    }

    switch (item->type) {
    case DBD_TYPE_VARIANT:
        return g_variant_print_string(item->variant, NULL, FALSE);
    case DBD_TYPE_LIST: {
        GString *str = g_string_new("{");
        gboolean first = TRUE;
        for (int i = 0; i < item->length; ++i) {
            GString *tmp = dbd_item_dump(item->list[i].item, NULL, TRUE);
            if (first) {
                g_string_append_printf(str, "'%s': %s", item->list[i].key, tmp->str);
                first = FALSE;
            } else {
                g_string_append_printf(str, ", '%s': %s", item->list[i].key, tmp->str);
            }
            g_free(tmp);
        }
        g_string_append_c(str, '}');
        return str;
    }
    case DBD_TYPE_TABLE: {
        if (!listMode) {
            return dbd_table_dump(item->table, path);
        }
        GString *str = g_string_new("{");
        GHashTableIter iter;
        gchar *key;
        GVariantTableItem *value;
        g_hash_table_iter_init(&iter, item->table);

        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GString *tmp = dbd_item_dump(value, NULL, TRUE);
            g_string_append_printf(str, "'%s': %s", key, tmp->str);
            g_free(tmp);
        }

        g_string_append_c(str, '}');
        return str;
    }
    }
}
GVariantTableItem *dbd_item_ref(GVariantTableItem *item)
{
    ++item->refcount;
    return item;
}
void dbd_item_unref(GVariantTableItem *item)
{
    if (!--item->refcount) {
        dbd_item_clear(item);
        g_free(item);
    }
}
