#include "inttypes.h"
#include "private_gvdb_common.h"

#ifndef LIBDBDCONF_PRIVATE_GVDB_EXPORT
#define LIBDBDCONF_PRIVATE_GVDB_EXPORT

typedef struct GvdbBuilder_t
{
    GQueue *chunks;
    gsize offset;
} GvdbBuilder;

typedef struct BuilderChunk_t
{
    gpointer data;
    gsize size;
    gsize offset;
} BuilderChunk;

typedef struct BucketCounter_t
{
    guint32 *buckets;
    guint32 n_buckets;
} BucketCounter;

static BucketCounter *dbd_bucketcounter_new(guint32 n_buckets)
{
    if (!n_buckets) {
        return NULL;
    }

    BucketCounter *counter = g_slice_new(BucketCounter);
    counter->n_buckets = n_buckets;
    counter->buckets = g_new0(guint32, n_buckets);
    return counter;
}

static guint32 dbd_bucketcounter_add(BucketCounter *bucket_counter, guint32 hash)
{
    if (!bucket_counter) {
        return -1;
    }
    hash %= bucket_counter->n_buckets;
    return bucket_counter->buckets[hash]++;
}

static guint32 dbd_bucketcounter_get(BucketCounter *bucket_counter, guint32 bucket)
{
    g_assert(bucket < bucket_counter->n_buckets);
    return bucket_counter->buckets[bucket];
}

static void dbd_bucketcounter_free(BucketCounter *bucket_counter)
{
    if (!bucket_counter) {
        return;
    }
    g_free(bucket_counter->buckets);
    g_free(bucket_counter);
}

static gboolean dbd_bucketcounter_counter_table(BucketCounter *bucket_counter,
                                                GVariantTableItem *item);
static gboolean dbd_bucketcounter_counter_list(BucketCounter *bucket_counter,
                                               GVariantTableItem *item)
{
    if (!bucket_counter) {
        return FALSE;
    }
    if (item->type != DBD_TYPE_LIST) {
        return FALSE;
    }
    for (guint32 i = 0; i < item->length; ++i) {
        dbd_bucketcounter_add(bucket_counter, dbd_hash(item->list->key, NULL));
        if (item->list[i].item->type == DBD_TYPE_LIST) {
            if (!dbd_bucketcounter_counter_list(bucket_counter, item->list[i].item)) {
                return FALSE;
            }
            break;
        }
    }
    return TRUE;
}

static gboolean dbd_bucketcounter_counter_table(BucketCounter *bucket_counter,
                                                GVariantTableItem *item)
{
    if (bucket_counter == NULL || item->type != DBD_TYPE_TABLE) {
        return FALSE;
    }

    GHashTableIter iter;
    gchar *key;
    GVariantTableItem *value;

    g_hash_table_iter_init(&iter, item->table);

    while (g_hash_table_iter_next(&iter, (gpointer*)&key, (gpointer*)&value)) {
        dbd_bucketcounter_add(bucket_counter, dbd_hash(key, NULL));
        if (value->type == DBD_TYPE_LIST) {
            if (!dbd_bucketcounter_counter_list(bucket_counter, value)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static guint32 dbd_bucketcounter_get_item_index(BucketCounter *counter, const guint32_le *buckets,
                                                guint32 hash)
{
    guint32 bucket;
    guint32 bucket_index;
    guint32 index;
    bucket = hash % counter->n_buckets;
    bucket_index = dbd_bucketcounter_add(counter, hash);
    index = bucket_index + guint32_from_le(buckets[bucket]);
    return index;
}

static GvdbBuilder *dbd_gvdbbuilder_new()
{
    GvdbBuilder *builder;

    builder = g_slice_new(GvdbBuilder);
    builder->chunks = g_queue_new();
    builder->offset += sizeof(struct gvdb_header);

    return builder;
}

static void dbd_gvdbbuilder_free(GvdbBuilder *builder)
{
    if (!builder) {
        return;
    }
    g_queue_free(builder->chunks);
    g_slice_free(GvdbBuilder, builder);
}

static gboolean dbd_gvdbbuilder_add_string(GvdbBuilder *builder, const gchar *string,
                                           guint32_le *start, guint16_le *size)
{
    BuilderChunk *chunk;
    gsize length;

    length = strlen(string);

    if (length > UINT16_MAX) {
        return FALSE;
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
    return TRUE;
}

static gpointer dbd_gvdbbuilder_allocate_chunk(GvdbBuilder *builder, guint alignment, gsize size,
                                               struct gvdb_pointer *pointer)
{
    if (!size || !builder || !alignment) {
        return NULL;
    }

    BuilderChunk *chunk = g_slice_new(BuilderChunk);
    builder->offset += (guint64)(-builder->offset) & (alignment - 1);
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

static gboolean dbd_gvdbbuilder_add_variant(GvdbBuilder *builder, GVariantTableItem *item,
                                            gboolean byteswap, guint32 *item_index,
                                            BucketCounter *counter, const guint32_le *buckets,
                                            const gchar *key, guint32_le parent,
                                            struct gvdb_hash_item *hash_item)
{
    GVariant *variant, *normal;
    gpointer data;
    guint32 hash;
    guint32 bucket;
    guint32 bucket_index;
    guint32 index;
    gsize size;

    if (!builder || !item || item->type != DBD_TYPE_VARIANT || !counter || !buckets || !hash_item) {
        return FALSE;
    }

    if (byteswap) {
        normal = g_variant_byteswap(item->variant);
        variant = g_variant_new_variant(normal);
        g_variant_unref(normal);
        normal = NULL;
    } else {
        variant = g_variant_new_variant(item->variant);
    }
    hash = dbd_hash(key, NULL);
    index = dbd_bucketcounter_get_item_index(counter, buckets, hash);

    hash_item[index].hash_value = guint32_to_le(hash);
    hash_item[index].parent = parent;
    hash_item[index].type = dbd_item_type_to_char(DBD_TYPE_VARIANT);

    if (!dbd_gvdbbuilder_add_string(builder, key, &hash_item[index].key_start,
                                    &hash_item[index].key_size)) {
        g_variant_unref(variant);
        return FALSE;
    }

    normal = g_variant_get_normal_form(variant);
    g_variant_unref(variant);

    size = g_variant_get_size(normal);
    data = dbd_gvdbbuilder_allocate_chunk(builder, 8, size, &hash_item[index].value.pointer);
    g_variant_store(normal, data);
    g_variant_unref(normal);
    ++(*item_index);
    return TRUE;
}

static gboolean dbd_gvdbbuilder_add_list(GvdbBuilder *builder, GVariantTableItem *list,
                                         gboolean byteswap, guint32 *item_index,
                                         BucketCounter *counter, const guint32_le *buckets,
                                         const gchar *key, guint32_le parent,
                                         struct gvdb_hash_item *hash_item)
{
    if (!builder || !list || list->type != DBD_TYPE_LIST || !counter || !buckets || !hash_item) {
        return FALSE;
    }

    guint32_le current_index = guint32_to_le((*item_index)++);
    guint32 hash = dbd_hash(key, NULL);
    guint32 index = dbd_bucketcounter_get_item_index(counter, buckets, hash);
    guint32_le *list_content;

    hash_item[index].hash_value = guint32_to_le(hash);
    hash_item[index].parent = parent;
    hash_item[index].type = dbd_item_type_to_char(DBD_TYPE_LIST);

    if (!dbd_gvdbbuilder_add_string(builder, key, &hash_item[index].key_start,
                                    &hash_item[index].key_size)) {
        return FALSE;
    }

    list_content = dbd_gvdbbuilder_allocate_chunk(builder, 4, 4 * list->length,
                                                  &hash_item[index].value.pointer);

    for (int i = 0; i < list->length; ++i) {
        list_content[i] = guint32_to_le(*item_index);
        switch (list->list[i].item->type) {
        case DBD_TYPE_VARIANT:
            dbd_gvdbbuilder_add_variant(builder, list->list[i].item, byteswap, item_index, counter,
                                        buckets, list->list[i].key, current_index, hash_item);
            break;
        case DBD_TYPE_LIST:
            dbd_gvdbbuilder_add_list(builder, list->list[i].item, byteswap, item_index, counter,
                                     buckets, list->list[i].key, current_index, hash_item);
            break;
        }
    }
    return TRUE;
}

static gboolean dbd_gvdbbuilder_add_table(GvdbBuilder *builder, GVariantTableItem *table,
                                          gboolean byteswap, guint32 *item_index,
                                          BucketCounter *counter, const guint32_le *buckets,
                                          const gchar *key, guint32_le parent,
                                          struct gvdb_hash_item *hash_item);

static gboolean dbd_gvdbbuilder_add_table_content(GvdbBuilder *builder, GVariantTableItem *table,
                                                  gboolean byteswap, struct gvdb_pointer *pointer)
{
    if (!builder || !table || table->type != DBD_TYPE_TABLE) {
        return FALSE;
    }

    guchar *data;
    guint32_le *bloom_filter, *hash_buckets;
    struct gvdb_hash_item *hash_items;
    BucketCounter *buckets_items = dbd_bucketcounter_new(table->childs);
    GHashTableIter iter;

    const gsize bloom_shift = 5;
    const gsize n_bloom_words = 0;
    const guint32_le bloom_hdr = guint32_to_le(bloom_shift << 27 | n_bloom_words);
    const guint32_le table_hdr = guint32_to_le(table->childs);

    gsize size = sizeof bloom_hdr + sizeof table_hdr + n_bloom_words * sizeof(guint32_le)
            + table->childs * sizeof(guint32_le) + table->childs * sizeof(struct gvdb_hash_item);

    gchar *key;
    GVariantTableItem *item;
    guint32 item_index = 0;

    dbd_bucketcounter_counter_table(buckets_items, table);

    data = dbd_gvdbbuilder_allocate_chunk(builder, 4, size, pointer);

#define chunk(s) (size -= (s), data += (s), data - (s))
    memcpy(chunk(sizeof bloom_hdr), &bloom_hdr, sizeof bloom_hdr);
    memcpy(chunk(sizeof table_hdr), &table_hdr, sizeof table_hdr);
    bloom_filter = (guint32_le *)chunk(n_bloom_words * sizeof(guint32_le));
    hash_buckets = (guint32_le *)chunk(table->childs * sizeof(guint32_le));
    hash_items = (struct gvdb_hash_item *)chunk(table->childs * sizeof(struct gvdb_hash_item));
    g_assert(size == 0);
#undef chunk

    memset(hash_buckets, 0, table->childs * sizeof(guint32_le));
    memset(hash_items, 0, table->childs * sizeof(guint32_le));

    for (int i = 1; i < table->childs; ++i) {
        hash_buckets[i] = guint32_to_le(guint32_from_le(hash_buckets[i - 1]) + dbd_bucketcounter_get(buckets_items, i - 1));
    }

    dbd_bucketcounter_free(buckets_items);
    buckets_items = dbd_bucketcounter_new(table->childs);
    g_hash_table_iter_init(&iter, table->table);

    while (g_hash_table_iter_next(&iter, (gpointer*)&key, (gpointer*)&item)) {
        switch (item->type) {
        case DBD_TYPE_LIST:
            if (!dbd_gvdbbuilder_add_list(builder, item, byteswap, &item_index, buckets_items,
                                          hash_buckets, key, guint32_to_le(-1), hash_items)) {
                return FALSE;
            }
            break;
        case DBD_TYPE_VARIANT:
            if (!dbd_gvdbbuilder_add_variant(builder, item, byteswap, &item_index, buckets_items,
                                             hash_buckets, key, guint32_to_le(-1), hash_items)) {
                return FALSE;
            }
            break;
        case DBD_TYPE_TABLE:
            if (!dbd_gvdbbuilder_add_table(builder, item, byteswap, &item_index, buckets_items,
                                           hash_buckets, key, guint32_to_le(-1), hash_items)) {
                return FALSE;
            }
            break;
        }
    }

    return TRUE;
}

static gboolean dbd_gvdbbuilder_add_table(GvdbBuilder *builder, GVariantTableItem *table,
                                          gboolean byteswap, guint32 *item_index,
                                          BucketCounter *counter, const guint32_le *buckets,
                                          const gchar *key, guint32_le parent,
                                          struct gvdb_hash_item *hash_item)
{
    if (!builder || !table || table->type != DBD_TYPE_TABLE || !counter || !buckets || !hash_item) {
        return FALSE;
    }

    guint32_le current_index = guint32_to_le((*item_index)++);
    guint32 hash = dbd_hash(key, NULL);
    guint32 index = dbd_bucketcounter_get_item_index(counter, buckets, hash);

    hash_item[index].hash_value = guint32_to_le(hash);
    hash_item[index].parent = parent;
    hash_item[index].type = dbd_item_type_to_char(DBD_TYPE_TABLE);

    if (!dbd_gvdbbuilder_add_string(builder, key, &hash_item[index].key_start,
                                    &hash_item[index].key_size)) {
        return FALSE;
    }

    return dbd_gvdbbuilder_add_table_content(builder, table, byteswap,
                                             &hash_item[index].value.pointer);
}

static GString *dbd_gvdbbuilder_serialize(GvdbBuilder *builder, gboolean byteswap,
                                          struct gvdb_pointer root)
{
    struct gvdb_header header;
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
    result = g_string_new_len((const gchar*)&header, sizeof header);

    if (!builder) {
        return result;
    }

    while (!g_queue_is_empty(builder->chunks)) {
        BuilderChunk *chunk = g_queue_pop_head(builder->chunks);

        if (result->len != chunk->offset) {
            gchar zero[8] = {
                0,
            };

            g_assert(chunk->offset > result->len);
            g_assert(chunk->offset - result->len < 8);

            g_string_append_len(result, zero, chunk->offset - result->len);
            g_assert(result->len == chunk->offset);
        }

        g_string_append_len(result, chunk->data, chunk->size);
        g_free(chunk->data);
        g_slice_free(BuilderChunk, chunk);
    }

    return result;
}

GBytes *dbd_table_get_raw(GVariantTableItem *table, gboolean byteswap, GError **error)
{
    if (!table || table->type != DBD_TYPE_TABLE) {
        return NULL;
    }

    struct gvdb_pointer root;
    GvdbBuilder *builder;
    GString *str;
    GBytes *res;
    gsize str_len;

    builder = dbd_gvdbbuilder_new();
    dbd_gvdbbuilder_add_table_content(builder, table, byteswap, &root);
    str = dbd_gvdbbuilder_serialize(builder, byteswap, root);
    str_len = str->len;

    res = g_bytes_new_take(g_string_free(str, FALSE), str_len);

    dbd_gvdbbuilder_free(builder);

    return res;
}

gboolean dbd_table_write_to_file(GVariantTableItem *table, const gchar *filename, gboolean byteswap,
                                 GError **error)
{
    if (!table || table->type != DBD_TYPE_TABLE || filename == NULL) {
        return FALSE;
    }
    GBytes *content;
    gboolean status;

    content = dbd_table_get_raw(table, byteswap, error);

    status = g_file_set_contents(filename, g_bytes_get_data(content, NULL),
                                 g_bytes_get_size(content), error);

    g_bytes_unref(content);

    return status;
}

#endif // LIBDBDCONF_PRIVATE_GVDB_EXPORT
