#ifndef LIBDBDCONF_PRIVATE_GVDB
#include <glib.h>
#define LIBDBDCONF_PRIVATE_GVDB

/// @brief Item of GVDB table.
typedef struct GVariantTableItem_t GVariantTableItem;

/// @brief List element of List Item in DVDB table.
typedef struct GVariantListElement_t {
    gchar* key;
    GVariantTableItem *item;
} GVariantListElement;

/// @brief All GVDB element types.
typedef enum GVariantTableItemType_t {
    /// @brief NONE type (is't valid).
    DBD_TYPE_NONE = 0,
    /// @brief VARIANT type. Equivalent of GVDB 'v'.
    DBD_TYPE_VARIANT = 1,
    /// @brief HASHTABLE type. Equivalent of GVDB 'H'.
    DBD_TYPE_TABLE = 2,
    /// @brief LIST type. Equivalent of GVDB 'L'.
    DBD_TYPE_LIST = 3,
} GVariantTableItemType;

/// @brief Create new table item.
/// @return new table item.
GVariantTableItem *dbd_table_new();
/// @brief Create new table item and then load into it GVDB from file.
/// @param filename GVDB layer file path.
/// @param trusted is trusted GVariant parse.
/// @param error handler.
/// @return new table item, or NULL.
GVariantTableItem *dbd_table_read_from_file(const gchar *filename, gboolean trusted, GError **error);
/// @brief Create new table item and then load into it GVDB from bytes.
/// @param data GVDB layer bytes.
/// @param trusted is trusted GVariant parse.
/// @param error handler.
/// @return new table item, or NULL.
GVariantTableItem *dbd_table_read_from_bytes(GBytes *data, gboolean trusted, GError **error);
/// @brief Add/set table key to value
/// @param table - current table. If table is't table or NULL, then do nothing.
/// @param key - key, for set.
/// @param value - value, for set. If NULL <=> dbd_table_unset.
/// @return current table.
GVariantTableItem *dbd_table_set(GVariantTableItem *table, const gchar *key, GVariantTableItem *value);
/// @brief Get table item by key. 
/// @param table - current table.
/// @param key - key of value.
/// @return item, or NULL.
GVariantTableItem *dbd_table_get(GVariantTableItem *table, const gchar *key);
/// @brief Remove table item by key.
/// @param table - current table.
/// @param key - key of value
/// @return current table, or NULL(if not founded, or if table is't table or NULL).
GVariantTableItem *dbd_table_unset(GVariantTableItem *table, const gchar *key);
/// @brief Write table into GVDB file.
/// @param filename GVDB layer file path(create, if does't exist).
/// @param byteswap - byteswap GVariant values.
/// @param error handler
/// @return if successful return TRUE, else FALSE.
gboolean dbd_table_write_to_file(GVariantTableItem *table, const gchar *filename, gboolean byteswap, GError **error);
/// @brief Write table into GVDB bytes.
/// @param byteswap - byteswap GVariant values.
/// @param error handler
/// @return if successful return new GBytes* with GVDB, else NULL.
GBytes *dbd_table_get_raw(GVariantTableItem *table, gboolean byteswap, GError **error);
/// @brief Dump table content in pretty string.
/// @param tablePath - table current path(or NULL)(Must begin and end with '/', or be "/")(no validations).
/// @return Pretty string for output.
GString *dbd_table_dump(GVariantTableItem *table, gchar *tablePath);

/// @brief Create new empty item.
/// @return New empty item (DBD_TYPE_NONE).
GVariantTableItem *dbd_item_new();
/// @brief Set item value to list.
/// @param item - current item.
/// @param list - array of list elements.
/// @param length - list length.
/// @return return current item.
GVariantTableItem *dbd_item_set_list(GVariantTableItem *item, GVariantListElement* list,
                                     guint32 length);
/// @brief Set item value to list, or append to, if it allready non-empty list.
/// @param item - current item.
/// @param list - array of list elements.
/// @param length - list length.
/// @return return current item.
GVariantTableItem *dbd_item_list_append(GVariantTableItem *item, GVariantListElement* list,
                                     guint32 length);
/// @brief Set item value to list, or append to, if it allready non-empty list.
/// @param item - current item.
/// @param element - single element to append.
/// @return return current item.
GVariantTableItem *dbd_item_list_append_element(GVariantTableItem *item, GVariantListElement element);
/// @brief Remove item from list by key.
/// @param item - current item.
/// @param element - element key.
/// @return if successful return current item, or NULL.
GVariantTableItem *dbd_item_list_remove_element(GVariantTableItem *item, const gchar* element);
/// @brief Remove items from list by keys. 
/// @param item - current item.
/// @param elements - elements keys.
/// @param nonexist_cancel - if TRUE even one key is missing, the operation will be canceled.
/// @return if successful return current item, or NULL.
GVariantTableItem *dbd_item_list_remove_elements(GVariantTableItem *item, const gchar** elements, gboolean nonexist_cancel);
/// @brief Clear current list to empty state.
/// @param item - current item.
/// @return current item.
GVariantTableItem *dbd_item_list_clear(GVariantTableItem *item);
/// @brief Set item variant.
/// @param item - current item.
/// @param variant - variant, to be setted.
/// @return current item.
GVariantTableItem *dbd_item_set_variant(GVariantTableItem *item, GVariant *variant);
/// @brief Get item list.
/// @param item - current item.
/// @param length - pointer to length result.
/// @return array of list element(parent owner) and list length by length pointer, or NULL.
GVariantListElement *dbd_item_get_list(GVariantTableItem *item, gsize *length);
/// @brief Get item variant.
/// @param item - current variant.
/// @return variant or NULL.
GVariant *dbd_item_get_variant(GVariantTableItem *item);
/// @brief Get item type
/// @param item - current item.
/// @return current item type.
GVariantTableItemType dbd_item_get_type(GVariantTableItem *item);
/// @brief Dump current item into pretty string.
/// @param item - current item.
/// @param path - table current path(or NULL)(Must begin and end with '/', or be "/")(no validations).
/// @param valueMode - true => dump like it value, false => dump like table(if item type is table).
/// @return Pretty output string.
GString* dbd_item_dump(GVariantTableItem *item, gchar* path, gboolean valueMode);
/// @brief Increase refcounter for current item.
/// @param item - current item.
/// @return current item.
GVariantTableItem *dbd_item_ref(GVariantTableItem *item);
/// @brief Decrease refcounter for current item.
/// @param item - current item.
void dbd_item_unref(GVariantTableItem *item);

#endif // LIBDBDCONF_PRIVATE_GVDB
