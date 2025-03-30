#ifndef LIBSVDB_PRIVATE_SVDB_EXPORT
#include "inttypes.h"
#include "private_svdb_common.c"
#define LIBSVDB_PRIVATE_SVDB_EXPORT

typedef struct GvdbBuilder_t {
    GQueue *chunks;
    gsize offset;
} GvdbBuilder;

typedef struct BuilderChunk_t {
    gpointer data;
    gsize size;
    gsize offset;
} BuilderChunk;

typedef struct BucketCounter_t {
    guint32 *buckets;
    guint32 n_buckets;
} BucketCounter;

static BucketCounter *svdb_bucketcounter_new(guint32 n_buckets) {
    if (!n_buckets) {
        return NULL;
    }

    BucketCounter *counter = g_slice_new(BucketCounter);
    counter->n_buckets = n_buckets;
    counter->buckets = g_new0(guint32, n_buckets);
    return counter;
}

static guint32 svdb_bucketcounter_add(BucketCounter *bucket_counter, guint32 hash) {
    if (!bucket_counter) {
        return -1;
    }
    hash %= bucket_counter->n_buckets;
    return bucket_counter->buckets[hash]++;
}

static guint32 svdb_bucketcounter_get(BucketCounter *bucket_counter, guint32 bucket) {
    if (bucket >= bucket_counter->n_buckets) {
        return -1;
    }
    return bucket_counter->buckets[bucket];
}

static void svdb_bucketcounter_free(BucketCounter *bucket_counter) {
    if (!bucket_counter) {
        return;
    }
    g_free(bucket_counter->buckets);
    g_free(bucket_counter);
}

static gboolean svdb_bucketcounter_counter_table(BucketCounter *bucket_counter,
                                                 SvdbTableItem *item);

static gboolean svdb_bucketcounter_counter_list(BucketCounter *bucket_counter,
                                                SvdbTableItem *item) {
    if (!bucket_counter) {
        return FALSE;
    }
    if (item->type != SVDB_TYPE_LIST) {
        return FALSE;
    }
    for (guint32 i = 0; i < item->length; ++i) {
        svdb_bucketcounter_add(bucket_counter, svdb_hash(item->list[i].key, NULL));
        if (item->list[i].item->type == SVDB_TYPE_LIST) {
            if (!svdb_bucketcounter_counter_list(bucket_counter, item->list[i].item)) {
                return FALSE;
            }
            break;
        }
    }
    return TRUE;
}

static gboolean svdb_bucketcounter_counter_table(BucketCounter *bucket_counter,
                                                 SvdbTableItem *item) {
    if (bucket_counter == NULL || item->type != SVDB_TYPE_TABLE) {
        return FALSE;
    }

    GHashTableIter iter;
    gchar *key;
    SvdbTableItem *value;

    g_hash_table_iter_init(&iter, item->table);

    while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &value)) {
        svdb_bucketcounter_add(bucket_counter, svdb_hash(key, NULL));
        if (value->type == SVDB_TYPE_LIST) {
            if (!svdb_bucketcounter_counter_list(bucket_counter, value)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static guint32 svdb_bucketcounter_get_item_index(BucketCounter *counter, const guint32_le *buckets,
                                                 guint32 hash) {
    guint32 bucket;
    guint32 bucket_index;
    guint32 index;
    bucket = hash % counter->n_buckets;
    bucket_index = svdb_bucketcounter_add(counter, hash);
    index = bucket_index + guint32_from_le(buckets[bucket]);
    return index;
}

static GvdbBuilder *svdb_gvdbbuilder_new() {
    GvdbBuilder *builder;

    builder = g_slice_new(GvdbBuilder);
    builder->chunks = g_queue_new();
    builder->offset = sizeof(struct svdb_header);

    return builder;
}

static void svdb_gvdbbuilder_free(GvdbBuilder *builder) {
    if (!builder) {
        return;
    }
    g_queue_free(builder->chunks);
    g_slice_free(GvdbBuilder, builder);
}

static void svdb_gvdbbuilder_add_string(GvdbBuilder *builder, const gchar *string,
                                        guint32_le *start, guint16_le *size, GError **error) {
    BuilderChunk *chunk;
    gsize length;

    length = strlen(string);

    if (length > UINT16_MAX) {
        g_set_error(error, SVDB_ERROR, 0, "%s%ld%s", "string length(", length,
                    ") greater than max string length(65535)");
        return;
    }

    chunk = g_slice_new0(BuilderChunk);
    chunk->offset = builder->offset;
    if (length) {
        chunk->size = length;
        chunk->data = g_strndup(string, length);
    }

    *start = guint32_to_le(builder->offset);
    *size = guint16_to_le(length);
    builder->offset += length;

    g_queue_push_tail(builder->chunks, chunk);
}

static gpointer svdb_gvdbbuilder_allocate_chunk(GvdbBuilder *builder, guint alignment, gsize size,
                                                struct svdb_pointer *pointer) {
    if (!size || !builder || !alignment) {
        return NULL;
    }

    BuilderChunk *chunk = g_slice_new(BuilderChunk);
    builder->offset += (guint64) (-builder->offset) & (alignment - 1);
    chunk->offset = builder->offset;
    chunk->size = size;
    chunk->data = g_malloc(size);

    if (pointer) {
        pointer->start = guint32_to_le(builder->offset);
        pointer->end = guint32_to_le(builder->offset + size);
    }

    builder->offset += size;
    g_queue_push_tail(builder->chunks, chunk);

    return chunk->data;
}

static guint32_le svdb_gvdbbuilder_add_variant(GvdbBuilder *builder, SvdbTableItem *item,
                                               gboolean byteswap, BucketCounter *counter, const guint32_le *buckets,
                                               const gchar *key, guint32_le parent,
                                               struct svdb_hash_item *hash_item, GError **error) {
    GVariant *variant, *normal;
    gpointer data;
    guint32 hash;
    guint32 bucket;
    guint32 bucket_index;
    guint32 index;
    gsize size;
    GError *tmp_error = NULL;

    if (!builder || !item || item->type != SVDB_TYPE_VARIANT || !counter || !buckets || !hash_item) {
        return guint32_to_le(-1);
    }

    hash = svdb_hash(key, NULL);
    index = svdb_bucketcounter_get_item_index(counter, buckets, hash);

    if (hash_item[index].hash_value.value != 0) {
        g_set_error_literal(error, SVDB_ERROR, 0, "internal error(collision while table building)");
        return guint32_to_le(-1);
    }
    hash_item[index].hash_value = guint32_to_le(hash);
    hash_item[index].parent = parent;
    hash_item[index].type = svdb_item_type_to_char(SVDB_TYPE_VARIANT);

    if (byteswap) {
        normal = g_variant_byteswap(item->variant);
        variant = g_variant_new_variant(normal);
        g_variant_unref(normal);
        normal = NULL;
    } else {
        variant = g_variant_new_variant(item->variant);
    }

    svdb_gvdbbuilder_add_string(builder, key, &hash_item[index].key_start,
                                &hash_item[index].key_size, &tmp_error);
    if (tmp_error) {
        g_variant_unref(variant);
        g_propagate_error(error, tmp_error);
        return guint32_to_le(-1);
    }

    normal = g_variant_get_normal_form(variant);
    g_variant_unref(variant);

    size = g_variant_get_size(normal);
    data = svdb_gvdbbuilder_allocate_chunk(builder, 8, size, &hash_item[index].value.pointer);
    g_variant_store(normal, data);
    g_variant_unref(normal);
    return guint32_to_le(index);
}

static guint32_le svdb_gvdbbuilder_add_list(GvdbBuilder *builder, SvdbTableItem *list,
                                            gboolean byteswap,
                                            BucketCounter *counter, const guint32_le *buckets,
                                            const gchar *key, guint32_le parent,
                                            struct svdb_hash_item *hash_item, GError **error) {
    if (!builder || !list || list->type != SVDB_TYPE_LIST || !counter || !buckets || !hash_item) {
        g_set_error_literal(error, SVDB_ERROR, 0, "internal error(trying add non-list item in add_list function)");
        return guint32_to_le(-1);
    }

    guint32 hash = svdb_hash(key, NULL);
    guint32 index = svdb_bucketcounter_get_item_index(counter, buckets, hash);
    guint32_le current_index = guint32_to_le(index);
    guint32_le *list_content;
    GError *tmp_error = NULL;

    if (hash_item[index].hash_value.value != 0) {
        g_set_error_literal(error, SVDB_ERROR, 0, "internal error(collision while table building)");
        return guint32_to_le(-1);
    }
    hash_item[index].hash_value = guint32_to_le(hash);
    hash_item[index].parent = parent;
    hash_item[index].type = svdb_item_type_to_char(SVDB_TYPE_LIST);

    svdb_gvdbbuilder_add_string(builder, key, &hash_item[index].key_start,
                                &hash_item[index].key_size, &tmp_error);
    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        return guint32_to_le(-1);
    }

    list_content = svdb_gvdbbuilder_allocate_chunk(builder, 4, 4 * list->length,
                                                   &hash_item[index].value.pointer);

    for (int i = 0; i < list->length; ++i) {
        switch (list->list[i].item->type) {
            case SVDB_TYPE_VARIANT:
                list_content[i] = svdb_gvdbbuilder_add_variant(builder, list->list[i].item, byteswap, counter,
                                                               buckets, list->list[i].key, current_index, hash_item,
                                                               &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    return guint32_to_le(-1);
                }
                break;
            case SVDB_TYPE_LIST:
                list_content[i] = svdb_gvdbbuilder_add_list(builder, list->list[i].item, byteswap, counter,
                                                            buckets, list->list[i].key, current_index, hash_item,
                                                            &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    return guint32_to_le(-1);
                }
                break;
        }
    }
    return current_index;
}

static guint32_le svdb_gvdbbuilder_add_table(GvdbBuilder *builder, SvdbTableItem *table,
                                             gboolean byteswap,
                                             BucketCounter *counter, const guint32_le *buckets,
                                             const gchar *key, guint32_le parent,
                                             struct svdb_hash_item *hash_item, GError **error);

static gboolean svdb_gvdbbuilder_add_table_content(GvdbBuilder *builder, SvdbTableItem *table,
                                                   gboolean byteswap, struct svdb_pointer *pointer, GError **error) {
    if (!builder || !table || table->type != SVDB_TYPE_TABLE) {
        g_set_error_literal(error, SVDB_ERROR, 0, "internal error(trying add non-table item in add_table function)");
        return FALSE;
    }

    guchar *data;
    guint32_le *bloom_filter, *hash_buckets;
    struct svdb_hash_item *hash_items;
    BucketCounter *buckets_items = svdb_bucketcounter_new(table->childs);
    GHashTableIter iter;
    GError *tmp_error = NULL;

    const gsize bloom_shift = 5;
    const gsize n_bloom_words = 0;
    const guint32_le bloom_hdr = guint32_to_le(bloom_shift << 27 | n_bloom_words);
    const guint32_le table_hdr = guint32_to_le(table->childs);

    gsize size = sizeof bloom_hdr + sizeof table_hdr + n_bloom_words * sizeof(guint32_le)
                 + table->childs * sizeof(guint32_le) + table->childs * sizeof(struct svdb_hash_item);

    gchar *key;
    SvdbTableItem *item;

    svdb_bucketcounter_counter_table(buckets_items, table);

    data = svdb_gvdbbuilder_allocate_chunk(builder, 4, size, pointer);

#define chunk(s) (size -= (s), data += (s), data - (s))
    memcpy(chunk(sizeof bloom_hdr), &bloom_hdr, sizeof bloom_hdr);
    memcpy(chunk(sizeof table_hdr), &table_hdr, sizeof table_hdr);
    bloom_filter = (guint32_le *) chunk(n_bloom_words * sizeof(guint32_le));
    hash_buckets = (guint32_le *) chunk(table->childs * sizeof(guint32_le));
    hash_items = (struct svdb_hash_item *) chunk(table->childs * sizeof(struct svdb_hash_item));
    if (size != 0) {
        g_set_error(error, SVDB_ERROR, 0, "internal error(table header size calculation error)");
    }
#undef chunk

    memset(hash_buckets, 0, table->childs * sizeof(guint32_le));
    memset(hash_items, 0, table->childs * sizeof(struct svdb_hash_item));

    for (int i = 1; i < table->childs; ++i) {
        const guint32 tmp_bucket = svdb_bucketcounter_get(buckets_items, i - 1);

        if (tmp_bucket == -1) {
            g_set_error_literal(error, SVDB_ERROR, 0,
                                "internal error(BucketCounter error)");
            return FALSE;
        }

        hash_buckets[i] = guint32_to_le(
            guint32_from_le(hash_buckets[i - 1]) + tmp_bucket);
    }

    svdb_bucketcounter_free(buckets_items);
    buckets_items = svdb_bucketcounter_new(table->childs);
    g_hash_table_iter_init(&iter, table->table);

    while (g_hash_table_iter_next(&iter, (gpointer *) &key, (gpointer *) &item)) {
        switch (item->type) {
            case SVDB_TYPE_LIST:
                svdb_gvdbbuilder_add_list(builder, item, byteswap, buckets_items,
                                          hash_buckets, key, guint32_to_le(-1), hash_items, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_bucketcounter_free(buckets_items);
                    return FALSE;
                }
                break;
            case SVDB_TYPE_VARIANT:
                svdb_gvdbbuilder_add_variant(builder, item, byteswap, buckets_items,
                                             hash_buckets, key, guint32_to_le(-1), hash_items, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_bucketcounter_free(buckets_items);
                    return FALSE;
                }
                break;
            case SVDB_TYPE_TABLE:
                svdb_gvdbbuilder_add_table(builder, item, byteswap, buckets_items,
                                           hash_buckets, key, guint32_to_le(-1), hash_items, &tmp_error);
                if (tmp_error) {
                    g_propagate_error(error, tmp_error);
                    svdb_bucketcounter_free(buckets_items);
                    return FALSE;
                }
                break;
        }
    }

    svdb_bucketcounter_free(buckets_items);
    return TRUE;
}

static guint32_le svdb_gvdbbuilder_add_table(GvdbBuilder *builder, SvdbTableItem *table,
                                             gboolean byteswap,
                                             BucketCounter *counter, const guint32_le *buckets,
                                             const gchar *key, guint32_le parent,
                                             struct svdb_hash_item *hash_item, GError **error) {
    if (!builder || !table || table->type != SVDB_TYPE_TABLE || !counter || !buckets || !hash_item) {
        return guint32_to_le(-1);
    }

    guint32 hash = svdb_hash(key, NULL);
    guint32 index = svdb_bucketcounter_get_item_index(counter, buckets, hash);
    guint32_le current_index = guint32_to_le(index);
    GError *tmp_error = NULL;

    if (hash_item[index].hash_value.value != 0) {
        g_set_error_literal(error, SVDB_ERROR, 0, "internal error(collision while table building)");
        return guint32_to_le(-1);
    }
    hash_item[index].hash_value = guint32_to_le(hash);
    hash_item[index].parent = parent;
    hash_item[index].type = svdb_item_type_to_char(SVDB_TYPE_TABLE);

    svdb_gvdbbuilder_add_string(builder, key, &hash_item[index].key_start,
                                &hash_item[index].key_size, &tmp_error);
    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        return guint32_to_le(-1);
    }

    svdb_gvdbbuilder_add_table_content(builder, table, byteswap,
                                       &hash_item[index].value.pointer, &tmp_error);
    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        return guint32_to_le(-1);
    }
    return current_index;
}

static GString *svdb_gvdbbuilder_serialize(GvdbBuilder *builder, gboolean byteswap,
                                           struct svdb_pointer root, GError **error) {
    struct svdb_header header;
    GString *result;

    memset(&header, 0, sizeof header);

    if (byteswap) {
        header.signature[0] = GVDB_SWAPPED_SIGNATURE0;
        header.signature[1] = GVDB_SWAPPED_SIGNATURE1;
    } else {
        header.signature[0] = GVDB_SIGNATURE0;
        header.signature[1] = GVDB_SIGNATURE1;
    }

    header.root = root;
    result = g_string_new_len((const gchar *) &header, sizeof header);

    if (!builder) {
        return result;
    }

    while (!g_queue_is_empty(builder->chunks)) {
        BuilderChunk *chunk = g_queue_pop_head(builder->chunks);

        if (result->len != chunk->offset) {
            gchar zero[8] = {
                0,
            };

            if (chunk->offset < result->len) {
                g_set_error_literal(error, SVDB_ERROR, 0,
                                    "internal error(chunk offset lesser than previous chunk end)");
                g_free(chunk->data);
                g_slice_free(BuilderChunk, chunk);
                goto error_exit;
            }
            if ((chunk->offset - result->len) >= 8) {
                g_set_error_literal(error, SVDB_ERROR, 0,
                                    "internal error(chunk offset greater than or equal to max align(8))");
                g_free(chunk->data);
                g_slice_free(BuilderChunk, chunk);
                goto error_exit;
            }

            g_string_append_len(result, zero, chunk->offset - result->len);

            if (result->len != chunk->offset) {
                g_set_error_literal(error, SVDB_ERROR, 0,
                                    "internal error(chunk offset greater than or equal to max align(8))");
                g_free(chunk->data);
                g_slice_free(BuilderChunk, chunk);
                goto error_exit;
            }
        }

        g_string_append_len(result, chunk->data, chunk->size);
        g_free(chunk->data);
        g_slice_free(BuilderChunk, chunk);
    }

    return result;

error_exit:
    g_string_free(result, TRUE);
    while (!g_queue_is_empty(builder->chunks)) {
        BuilderChunk *tmp = g_queue_pop_head(builder->chunks);
        g_free(tmp);
        g_slice_free(BuilderChunk, tmp);
    }
    return NULL;
}

GBytes *svdb_table_get_raw(SvdbTableItem *table, gboolean byteswap, GError **error) {
    if (!table || table->type != SVDB_TYPE_TABLE) {
        return NULL;
    }

    struct svdb_pointer root;
    GvdbBuilder *builder;
    GString *str;
    GBytes *res;
    gsize str_len;
    GError *tmp_error = NULL;

    builder = svdb_gvdbbuilder_new();
    svdb_gvdbbuilder_add_table_content(builder, table, byteswap, &root, &tmp_error);

    if (tmp_error) {
        svdb_gvdbbuilder_free(builder);
        g_propagate_error(error, tmp_error);
        return NULL;
    }

    str = svdb_gvdbbuilder_serialize(builder, byteswap, root, &tmp_error);

    if (tmp_error) {
        svdb_gvdbbuilder_free(builder);
        g_propagate_error(error, tmp_error);
        return NULL;
    }

    str_len = str->len;

    res = g_bytes_new_take(g_string_free(str, FALSE), str_len);

    svdb_gvdbbuilder_free(builder);
    return res;
}

gboolean svdb_table_write_to_file(SvdbTableItem *table, const gchar *filename, gboolean byteswap,
                                  GError **error) {
    if (!table || table->type != SVDB_TYPE_TABLE || filename == NULL) {
        return FALSE;
    }
    GBytes *content;
    gboolean status;

    content = svdb_table_get_raw(table, byteswap, error);

    status = g_file_set_contents(filename, g_bytes_get_data(content, NULL),
                                 g_bytes_get_size(content), error);

    g_bytes_unref(content);

    return status;
}

#endif // LIBSVDB_PRIVATE_SVDB_EXPORT
