#ifndef LIBSVDB_PRIVATE_SVDB_PARSE
#include "private_svdb_common.c"
#define LIBSVDB_PRIVATE_SVDB_PARSE

static gconstpointer svdb_table_dereference(gconstpointer file_data, gsize file_size,
                                            const struct svdb_pointer pointer, gint alignment,
                                            gsize *block_size) {
    guint32 start, end;

    start = guint32_from_le(pointer.start);
    end = guint32_from_le(pointer.end);

    if G_UNLIKELY(start > end || end > file_size || start & (alignment - 1)
        || (alignment & (alignment - 1)))
        return NULL;

    *block_size = end - start;

    return file_data + start;
}

static GVariant *svdb_gvdb_item_get_variant(gconstpointer block, gsize block_size, gboolean byteswap,
                                            gboolean trusted, const struct svdb_hash_item *item) {
    GVariant *variant, *value;
    gconstpointer data;
    GBytes *bytes;
    gsize size;

    data = svdb_table_dereference(block, block_size, item->value.pointer, 8, &size);

    if G_UNLIKELY(data == NULL) {
        return NULL;
    }

    bytes = g_bytes_new(data, size);
    variant = g_variant_new_from_bytes(G_VARIANT_TYPE_VARIANT, bytes, trusted);
    value = g_variant_get_variant(variant);
    g_variant_unref(variant);
    g_bytes_unref(bytes);

    if (byteswap) {
        GVariant *tmp = g_variant_byteswap(value);
        g_variant_unref(value);
        value = tmp;
    }

    return value;
}

static void svdb_item_set_parent(SvdbTableItem *item, SvdbTableItem *parent, GError **error) {
    if (!parent || !item) {
        return;
    }
    if (item->parent) {
        g_set_error_literal(error, SVDB_ERROR, 0, "trying to attach item to more than one parent.");
        return;
    }
    if (parent->type == SVDB_TYPE_VARIANT) {
        g_set_error_literal(error, SVDB_ERROR, 0, "trying to attach item to variant.");
        return;
    }
    item->parent = parent;
    guint32 count = item->childs + 1;

    while (parent && parent->type == SVDB_TYPE_LIST) {
        parent->childs += count;
        parent = parent->parent;
    }

    if (parent) {
        parent->childs += count;
    }
}


static gchar *svdb_gvdb_item_get_key(gconstpointer block, gsize block_size,
                                     const struct svdb_hash_item *item) {
    guint32 start, end, size;
    start = guint32_from_le(item->key_start);
    size = guint16_from_le(item->key_size);
    end = start + size;

    if G_UNLIKELY(start > end || end > block_size) {
        return NULL;
    }

    return g_strndup(block + start, size);
}

static gboolean svdb_parse_table_header(gconstpointer block, gsize block_size,
                                        const struct svdb_pointer table, SVDBTableHeader *header) {
    if (!header) {
        return FALSE;
    }
    const struct svdb_hash_header *gvdb_header;
    gsize size;

    memset(header, 0, sizeof *header);
    gvdb_header = svdb_table_dereference(block, block_size, table, 4, &size);

    if G_UNLIKELY(header == NULL || size < sizeof *gvdb_header) {
        return FALSE;
    }

    size -= sizeof *gvdb_header;

    header->n_buckets = guint32_from_le(gvdb_header->n_buckets);
    header->n_bloom_words = guint32_from_le(gvdb_header->n_bloom_words);
    header->bloom_shift = header->n_bloom_words >> 27;
    header->n_bloom_words &= (1u << 27) - 1;

    if G_UNLIKELY(header->n_bloom_words * sizeof *header->bloom_words > size) {
        return FALSE;
    }

    size -= header->n_bloom_words * sizeof *header->bloom_words;
    header->bloom_words = (gconstpointer) (gvdb_header + 1);

    if G_UNLIKELY(header->n_buckets > G_MAXUINT / sizeof *header->hash_buckets
        || header->n_buckets * sizeof *header->hash_buckets > size) {
        return FALSE;
    }

    header->hash_buckets = header->bloom_words + header->n_bloom_words;
    size -= header->n_buckets * sizeof(guint32_le);

    if G_UNLIKELY(size % sizeof *header->hash_items) {
        return FALSE;
    }

    header->hash_items = (gpointer) (header->hash_buckets + header->n_buckets);
    header->n_hash_items = size / sizeof *header->hash_items;

    return TRUE;
}

static gboolean svdb_table_list_indecies_from_item(gconstpointer block, gsize block_size,
                                                   const struct svdb_hash_item *item,
                                                   const guint32_le **list, guint *length) {
    gsize size;
    *list = svdb_table_dereference(block, block_size, item->value.pointer, 4, &size);

    if G_LIKELY(*list == NULL || size % 4)
        return FALSE;

    *length = size / 4;
    return TRUE;
}

static SvdbTableItem *svdb_parse_table_variant(SVDBTableHeader header, gconstpointer block,
                                               gsize block_size, gboolean byteswap,
                                               gboolean trusted,
                                               const struct svdb_hash_item *variant, GError **error) {
    if (variant->type != 'v') {
        g_set_error_literal(error, SVDB_ERROR, 0, "internal error(expected variant item)");
        return NULL;
    }

    GVariant *value = svdb_gvdb_item_get_variant(block, block_size, byteswap, trusted, variant);

    if (value == NULL) {
        g_set_error_literal(error, SVDB_ERROR, 0, "corrupted gvdb file");
        return NULL;
    }

    SvdbTableItem *item = svdb_item_new();

    svdb_item_set_variant(item, value);
    g_variant_unref(value);
    return item;
}

static SvdbTableItem *svdb_parse_table(gconstpointer block, gsize block_size, gboolean byteswap,
                                       gboolean trusted, const struct svdb_pointer table, GError **error);


static SvdbTableItem *svdb_parse_table_list(SVDBTableHeader header, gconstpointer block,
                                            gsize block_size, gboolean byteswap,
                                            gboolean trusted,
                                            const struct svdb_hash_item *list_item, GError **error) {
    if (list_item->type != 'L') {
        return NULL;
    }
    guint size;
    SvdbTableItem *item;
    const guint32_le *indecies;
    SvdbListElement *list_elements;
    SvdbListElement *curr;
    GError *tmp_error = NULL;

    if (!svdb_table_list_indecies_from_item(block, block_size, list_item, &indecies, &size)) {
        g_set_error_literal(error, SVDB_ERROR, 0, "corrupted gvdb file(corrupted list)");
        return NULL;
    }

    item = svdb_item_new();
    svdb_item_set_list(item, NULL, 0, &tmp_error);

    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        return NULL;
    }

    list_elements = g_malloc0(size * sizeof *list_elements);
    curr = list_elements;

    for (int i = 0; i < size; ++i) {
        guint32 itemno = guint32_from_le(indecies[i]);
        const struct svdb_hash_item *list_element;
        if (itemno > header.n_hash_items) {
            continue;
        }
        list_element = header.hash_items + itemno;
        curr->key = svdb_gvdb_item_get_key(block, block_size, list_element);
        switch (svdb_item_char_to_type(list_element->type)) {
            case SVDB_TYPE_VARIANT: {
                curr->item = svdb_parse_table_variant(header, block, block_size, byteswap, trusted,
                                                      list_element, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(item);
                    goto error_exit;
                }

                if (curr->item) {
                    ++curr;
                } else {
                    g_free(curr->key);
                }
                break;
            }
            case SVDB_TYPE_LIST: {
                curr->item = svdb_parse_table_list(header, block, block_size, byteswap, trusted,
                                                   list_element, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(item);
                    goto error_exit;
                }

                if (curr->item) {
                    ++curr;
                } else {
                    g_free(curr->key);
                }
                break;
            }
            case SVDB_TYPE_TABLE: {
                SvdbTableItem *table = svdb_parse_table(block, block_size, byteswap, trusted,
                                                        list_element->value.pointer, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(item);
                    goto error_exit;
                }

                curr->item = table;
                if (curr->item) {
                    ++curr;
                } else {
                    g_free(curr->key);
                }
                break;
            }
        }
    }

    svdb_item_set_list(item, list_elements, curr - list_elements, &tmp_error);
    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        goto error_exit;
    }

    while (curr > list_elements) {
        --curr;
        svdb_item_unref(curr->item);
        g_free(curr->key);
    }
    g_free(list_elements);

    return item;

error_exit:
    svdb_item_unref(item);
    while (curr > list_elements) {
        curr--;
        g_free(curr->key);
        svdb_item_unref(curr->item);
    }
    g_free(list_elements);
    return NULL;
}

static SvdbTableItem *svdb_parse_table(gconstpointer block, gsize block_size, gboolean byteswap,
                                       gboolean trusted, const struct svdb_pointer table, GError **error) {
    SvdbTableItem *result = svdb_table_new();
    SVDBTableHeader header;
    GError *tmp_error = NULL;

    if (!svdb_parse_table_header(block, block_size, table, &header)) {
        return NULL;
    }

    for (guint32 i = 0; i < header.n_hash_items; ++i) {
        if (guint32_from_le(header.hash_items[i].parent) != -1) {
            continue;
        }
        switch (svdb_item_char_to_type(header.hash_items[i].type)) {
            case SVDB_TYPE_VARIANT: {
                SvdbTableItem *item = svdb_parse_table_variant(header, block, block_size, byteswap,
                                                               trusted, header.hash_items + i, &tmp_error);

                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(result);
                    return NULL;
                }

                svdb_item_set_parent(item, result, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(item);
                    svdb_item_unref(result);
                    return NULL;
                }
                g_hash_table_insert(result->table,
                                    svdb_gvdb_item_get_key(block, block_size, header.hash_items + i),
                                    item);
                break;
            }
            case SVDB_TYPE_LIST: {
                SvdbTableItem *item = svdb_parse_table_list(header, block, block_size, byteswap,
                                                            trusted, header.hash_items + i, &tmp_error);

                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(result);
                    return NULL;
                }

                if (!item) {
                    continue;
                }

                svdb_item_set_parent(item, result, &tmp_error);

                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(item);
                    svdb_item_unref(result);
                    return NULL;
                }

                g_hash_table_insert(result->table,
                                    svdb_gvdb_item_get_key(block, block_size, header.hash_items + i),
                                    item);

                break;
            }
            case SVDB_TYPE_TABLE: {
                SvdbTableItem *value = svdb_parse_table(block, block_size, byteswap, trusted,
                                                        header.hash_items[i].value.pointer, &tmp_error);

                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(result);
                    return NULL;
                }

                svdb_item_set_parent(value, result, &tmp_error);

                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_item_unref(value);
                    svdb_item_unref(result);
                    return NULL;
                }

                g_hash_table_insert(result->table,
                                    svdb_gvdb_item_get_key(block, block_size, header.hash_items + i),
                                    value);
                break;
            }
        }
    }
    return result;
}
#endif // LIBSVDB_PRIVATE_SVDB_PARSE
