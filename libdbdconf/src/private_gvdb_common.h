#ifndef LIBDBDCONF_PRIVATE_GVDB_COMMON
#include <glib.h>
#include <libdbdconf/gvdb.h>
#define LIBDBDCONF_PRIVATE_GVDB_COMMON

typedef struct { guint16 value; } guint16_le;
typedef struct { guint32 value; } guint32_le;

struct gvdb_pointer {
  guint32_le start;
  guint32_le end;
};

struct gvdb_hash_header {
  guint32_le n_bloom_words;
  guint32_le n_buckets;
};

struct gvdb_hash_item {
  guint32_le hash_value;
  guint32_le parent;

  guint32_le key_start;
  guint16_le key_size;
  gchar type;
  gchar unused;

  union
  {
    struct gvdb_pointer pointer;
    gchar direct[8];
  } value;
};

struct gvdb_header {
  guint32 signature[2];
  guint32_le version;
  guint32_le options;

  struct gvdb_pointer root;
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

struct GVariantTableItem_t
{
    /// @brief Pointer to parent table/list(non-variant, and non-none).
    GVariantTableItem *parent;
    /// @brief Current type of item.
    GVariantTableItemType type;
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
            GVariantListElement *list;
            gsize length;
        };
    };
};

typedef struct DBDTableHeader_t
{
    struct gvdb_hash_item *hash_items;
    const guint32_le *bloom_words;
    const guint32_le *hash_buckets;
    guint32 n_bloom_words;
    guint bloom_shift;
    guint32 n_buckets;
    guint32 n_hash_items;
} DBDTableHeader;

static guint32 dbd_hash(const gchar *key, guint32 *keylength)
{
    if (!key || !*key) {
        if (keylength) {
            *keylength = 0;
        }
        return 5381;
    }

    guint32 hash_value = 5381;
    guint32 length;

    for (length = 0; key[length]; ++length) {
        hash_value = (hash_value * 33) + ((signed char *)key)[length];
    }

    if (keylength) {
        *keylength = length;
    }

    return hash_value;
}

static gchar dbd_item_type_to_char(GVariantTableItemType type)
{
    switch (type) {
    case DBD_TYPE_VARIANT:
        return 'v';
    case DBD_TYPE_TABLE:
        return 'H';
    case DBD_TYPE_LIST:
        return 'L';
    }
    return 0;
}
static GVariantTableItemType dbd_item_char_to_type(gchar character)
{
    switch (character) {
    case 'v':
        return DBD_TYPE_VARIANT;
    case 'H':
        return DBD_TYPE_TABLE;
    case 'L':
        return DBD_TYPE_LIST;
    }
    return DBD_TYPE_NONE;
}

static void dbd_dettach(GVariantTableItem *item)
{
    guint32 length = item->childs + 1;

    GVariantTableItem *parent = item->parent;

    while (parent && parent->type == DBD_TYPE_LIST) {
        parent->childs -= length;
        parent = parent->parent;
    }

    if (parent) {
        parent->childs -= length;
    }
}

static void dbd_item_clear_unref_dettach(GVariantTableItem *item) 
{
    dbd_dettach(item);
    dbd_item_unref(item);
}

static void dbd_item_clear(GVariantTableItem *item)
{
    switch (item->type) {
    case DBD_TYPE_VARIANT:
        g_variant_unref(item->variant);
        break;
    case DBD_TYPE_TABLE:
        g_hash_table_unref(item->table);
        break;
    case DBD_TYPE_LIST:
        for (int i = item->length; i > 0; --i) {
            dbd_dettach(item->list[i-1].item);
            dbd_item_unref(item->list[i - 1].item);
            g_free(item->list[i - 1].key);
        }
        g_free_sized(item->list, sizeof *(item->list) * item->length);
        break;
    }

    item->type = DBD_TYPE_NONE;
    guint32 old = item->childs;
    item->childs = 0;
    
    GVariantTableItem *parent = item->parent;

    while (parent && parent->type == DBD_TYPE_LIST) {
        parent->childs -= old;
        parent = parent->parent;
    }

    if (parent) {
        parent->childs -= old;
    }
}

static GVariantTableItem *dbd_item_set_table(GVariantTableItem *item, GHashTable *table)
{
    if (!item) {
        return NULL;
    }
    dbd_item_clear(item);

    if (!table) {
        return NULL;
    }

    item->type = DBD_TYPE_TABLE;
    item->table = g_hash_table_ref(table);
    return item;
}
#endif // LIBDBDCONF_PRIVATE_GVDB_COMMON 
