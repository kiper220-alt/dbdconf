#ifndef LIBSVDB_PRIVATE_SVDB_COMMON
#include <glib.h>
#include <svdb.h>
#include <gio/gio.h>
#include <glib-object.h>
#define LIBSVDB_PRIVATE_SVDB_COMMON

#ifndef __gvdb_format_h__
#define __gvdb_format_h__
typedef struct { guint16 value; } guint16_le;
typedef struct { guint32 value; } guint32_le;

#define SVDB_ERROR (svdb_error_quark())
GQuark svdb_error_quark(void);


struct svdb_pointer {
  guint32_le start;
  guint32_le end;
};

struct svdb_hash_header {
  guint32_le n_bloom_words;
  guint32_le n_buckets;
};

struct svdb_hash_item {
  guint32_le hash_value;
  guint32_le parent;

  guint32_le key_start;
  guint16_le key_size;
  gchar type;
  gchar unused;

  union
  {
    struct svdb_pointer pointer;
    gchar direct[8];
  } value;
};

struct svdb_header {
  guint32 signature[2];
  guint32_le version;
  guint32_le options;

  struct svdb_pointer root;
};

static inline guint32_le guint32_to_le (guint32 value) {
  guint32_le result = { GUINT32_TO_LE (value) };
  return result;
}

static inline guint32 guint32_from_le (guint32_le value) {
  return GUINT32_FROM_LE (value.value);
}

static inline guint16_le guint16_to_le (guint16 value) {
  guint16_le result = { GUINT16_TO_LE (value) };
  return result;
}

static inline guint16 guint16_from_le (guint16_le value) {
  return GUINT16_FROM_LE (value.value);
}

#define GVDB_SIGNATURE0 1918981703
#define GVDB_SIGNATURE1 1953390953
#define GVDB_SWAPPED_SIGNATURE0 GUINT32_SWAP_LE_BE (GVDB_SIGNATURE0)
#define GVDB_SWAPPED_SIGNATURE1 GUINT32_SWAP_LE_BE (GVDB_SIGNATURE1)

#endif

struct SvdbTableItem_t
{
    /// @brief Pointer to parent table/list(non-variant, and non-none).
    SvdbTableItem *parent;
    /// @brief Current type of item.
    SvdbItemType type;
    /// @brief Thread-unsafe refcounter.
    grefcount refcount;
    /// @brief One depth child count. If table => count of all child(non-recursive) + list length of
    /// child lists(recursive). If List => list length. If Variant => 0).
    guint32 childs;
    union {
        /// @brief Variant type.
        GVariant *variant;
        /// @brief Table type.
        GHashTable *table;
        /// @brief List type.
        struct
        {
            SvdbListElement *list;
            gsize length;
        };
    };
};

typedef struct SVDBTableHeader_t
{
    struct svdb_hash_item *hash_items;
    const guint32_le *bloom_words;
    const guint32_le *hash_buckets;
    guint32 n_bloom_words;
    guint bloom_shift;
    guint32 n_buckets;
    guint32 n_hash_items;
} SVDBTableHeader;

static guint32 svdb_hash(const gchar *key, guint32 *key_length)
{
    if (!key || !*key) {
        if (key_length) {
            *key_length = 0;
        }
        return 5381;
    }

    guint32 hash_value = 5381;
    guint32 length;

    for (length = 0; key[length]; ++length) {
        hash_value = (hash_value * 33) + ((signed char *)key)[length];
    }

    if (key_length) {
        *key_length = length;
    }

    return hash_value;
}

static gchar svdb_item_type_to_char(SvdbItemType type)
{
    switch (type) {
    case SVDB_TYPE_VARIANT:
        return 'v';
    case SVDB_TYPE_TABLE:
        return 'H';
    case SVDB_TYPE_LIST:
        return 'L';
    }
    return 0;
}
static SvdbItemType svdb_item_char_to_type(gchar character)
{
    switch (character) {
    case 'v':
        return SVDB_TYPE_VARIANT;
    case 'H':
        return SVDB_TYPE_TABLE;
    case 'L':
        return SVDB_TYPE_LIST;
    }
    return SVDB_TYPE_NONE;
}

static void svdb_dettach(SvdbTableItem *item)
{
    guint32 length = item->childs + 1;

    SvdbTableItem *parent = item->parent;

    while (parent && parent->type == SVDB_TYPE_LIST) {
        parent->childs -= length;
        parent = parent->parent;
    }

    if (parent) {
        parent->childs -= length;
    }

    item->parent = NULL;
}

static void svdb_item_clear_unref_dettach(SvdbTableItem *item)
{
    svdb_dettach(item);
    svdb_item_unref(item);
}

static void svdb_item_clear(SvdbTableItem *item)
{
    switch (item->type) {
    case SVDB_TYPE_VARIANT:
        g_variant_unref(item->variant);
        break;
    case SVDB_TYPE_TABLE:
        g_hash_table_unref(item->table);
        break;
    case SVDB_TYPE_LIST:
        for (gsize i = item->length; i > 0; --i) {
            svdb_dettach(item->list[i - 1].item);
            svdb_item_unref(item->list[i - 1].item);
            g_free(item->list[i - 1].key);
        }
        g_free_sized(item->list, sizeof *(item->list) * item->length);
        break;
    }

    item->type = SVDB_TYPE_NONE;
    guint32 old = item->childs;
    item->childs = 0;
    
    SvdbTableItem *parent = item->parent;

    while (parent && parent->type == SVDB_TYPE_LIST) {
        parent->childs -= old;
        parent = parent->parent;
    }

    if (parent) {
        parent->childs -= old;
    }
}

static SvdbTableItem *svdb_item_set_table(SvdbTableItem *item, GHashTable *table)
{
    if (!item) {
        return NULL;
    }
    svdb_item_clear(item);

    if (!table) {
        return NULL;
    }

    item->type = SVDB_TYPE_TABLE;
    item->table = g_hash_table_ref(table);
    return item;
}
#endif // LIBSVDB_PRIVATE_SVDB_COMMON
