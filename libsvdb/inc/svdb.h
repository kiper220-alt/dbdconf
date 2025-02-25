#ifndef LIBSVDB_PRIVATE_GVDB

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

#define LIBSVDB_PRIVATE_GVDB

#define SVDB_TABLE_ITEM_TYPE        \
    (svdb_table_get_type())
#define SVDB_TABLE_ITEM(o)            \
    (G_TYPE_CHECK_INSTANCE_CAST ((o), SVDB_TABLE_ITEM_TYPE, SvdbTableItem))
#define SVDB_IS_TABLE_ITEM(o)        \
    (G_TYPE_CHECK_INSTANCE_TYPE ((o), SVDB_TABLE_ITEM_TYPE))
#define TUT_IS_GREETER_CLASS(c)        \
    (G_TYPE_CHECK_CLASS_TYPE ((c),  TUT_GREETER_TYPE))

/// @brief Get type function for GIR and Typelib.
GType svdb_table_get_type(void);

/// @brief Item of GVDB table.
typedef struct SvdbTableItem_t SvdbTableItem;

/// @brief List element of List Item in DVDB table.
typedef struct SvdbListElement_t {
    gchar *key;
    SvdbTableItem *item;
} SvdbListElement;

/// @brief All GVDB element types.
typedef enum SvdbItemType {
    /// @brief NONE type (is't valid).
    SVDB_TYPE_NONE = 0,
    /// @brief VARIANT type. Equivalent of GVDB 'v'.
    SVDB_TYPE_VARIANT = 1,
    /// @brief HASHTABLE type. Equivalent of GVDB 'H'.
    SVDB_TYPE_TABLE = 2,
    /// @brief LIST type. Equivalent of GVDB 'L'.
    SVDB_TYPE_LIST = 3,
} SvdbItemType;


/// @brief Create new table item.
/// @return new table item.
SvdbTableItem *svdb_table_new(void);

/// @brief Create new table item and then load into it GVDB from file.
/// @param filename GVDB layer file path.
/// @param trusted is trusted GVariant parse.
/// @param error handler.
/// @return new table item, or NULL.
SvdbTableItem *svdb_table_read_from_file(const gchar *filename, gboolean trusted, GError **error);

/// @brief Create new table item and then load into it GVDB from bytes.
/// @param data GVDB layer bytes.
/// @param trusted is trusted GVariant parse.
/// @param error handler.
/// @return new table item, or NULL.
SvdbTableItem *svdb_table_read_from_bytes(GBytes *bytes, gboolean trusted, GError **error);

/// @brief Add/set table key to value
/// @param table - current table. If table is't table or NULL, then do nothing.
/// @param key - key, for set.
/// @param value - value, for set. If NULL <=> svdb_table_unset.
/// @return current table.
gboolean svdb_table_set(SvdbTableItem *table, const gchar *key, SvdbTableItem *value);

/// @brief Get table item by key. 
/// @param table - current table.
/// @param key - key of value.
/// @return item, or NULL.
SvdbTableItem *svdb_table_get(const SvdbTableItem *table, const gchar *key);

/// @brief Remove table item by key.
/// @param table - current table.
/// @param key - key of value
/// @return current table, or NULL(if not founded, or if table is't table or NULL).
gboolean svdb_table_unset(SvdbTableItem *table, const gchar *key);

/// @brief Write table into GVDB file.
/// @param filename GVDB layer file path(create, if does't exist).
/// @param byteswap - byteswap GVariant values.
/// @param error handler
/// @return if successful return TRUE, else FALSE.
gboolean svdb_table_write_to_file(SvdbTableItem *table, const gchar *filename, gboolean byteswap, GError **error);

/// @brief Write table into GVDB bytes.
/// @param byteswap - byteswap GVariant values.
/// @param error handler
/// @return if successful return new GBytes* with GVDB, else NULL.
GBytes *svdb_table_get_raw(SvdbTableItem *table, gboolean byteswap, GError **error);


/// @brief Return table child items.
/// @param table - table current path(or NULL).
/// @param size - pointer for return size(or NULL).
/// @return Array of strings with child names, or NULL(if no child is present or table isn't table :) ).
/// Value must be freed with `g_strfreev`
gchar **svdb_table_list_child(const SvdbTableItem *table, gsize *size);

/// @brief Create new empty item.
/// @return New empty item (SVDB_TYPE_NONE).
SvdbTableItem *svdb_item_new();

/// @brief Set item value to list.
/// @param item - current item.
/// @param list - array of list elements.
/// @param length - list length.
/// @return return current item.
gboolean svdb_item_set_list(SvdbTableItem *item, const SvdbListElement *list,
                                  guint32 length);

/// @brief Set item value to list, or append to, if it allready non-empty list.
/// @param item - current item.
/// @param list - array of list elements.
/// @param length - list length.
/// @return return current item.
gboolean svdb_item_list_append(SvdbTableItem *item, const SvdbListElement *list,
                                     guint32 length);

/// @brief Set list value to list, or append to, if it allready non-empty list.
/// @param list - current list.
/// @param element - single element to append.
/// @return return current list.
gboolean svdb_item_list_append_element(SvdbTableItem *list, SvdbListElement element);

/// @brief Set list value to list, or append to, if it allready non-empty list.
/// @param list - current list.
/// @param key - key of appended value.
/// @param variant - value of appended value.
/// @return return current list.
gboolean svdb_item_list_append_variant(SvdbTableItem *list, const gchar *key, GVariant *value);

/// @brief Set list value to list, or append to, if it allready non-empty list.
/// @param list - current list.
/// @param key - key of appended value.
/// @param value - value of appended value.
/// @return return current list.
gboolean svdb_item_list_append_value(SvdbTableItem *list, const gchar *key, SvdbTableItem *value);

/// @brief Remove item from list by key.
/// @param item - current item.
/// @param element - element key.
/// @return if successful return current item, or NULL.
gboolean svdb_item_list_remove_element(SvdbTableItem *item, const gchar *element);

/// @brief Remove items from list by keys. 
/// @param item - current item.
/// @param elements - elements keys.
/// @param nelements - elements keys count.
/// @param exist_cancel - if TRUE even one key is missing, the operation will be canceled. (+O(n*m) overhead, n - list count, m - elements count)
/// @return if successful return current item, or NULL.
gboolean svdb_item_list_remove_elements(SvdbTableItem *item, const gchar **elements, gsize nelements, gboolean exist_cancel);

/// @brief Clear current list to empty state.
/// @param item - current item.
/// @return current item.
gboolean svdb_item_list_clear(SvdbTableItem *item);

/// @brief Set item variant.
/// @param item - current item.
/// @param variant - variant, to be setted.
/// @return current item.
gboolean svdb_item_set_variant(SvdbTableItem *item, GVariant *variant);

/// @brief Get item list.
/// @param item - current item.
/// @param length - pointer to length result.
/// @return array of list element(parent owner) and list length by length pointer, or NULL.
const SvdbListElement *svdb_item_get_list(const SvdbTableItem *item, gsize *length);

/// @brief Get item variant.
/// @param item - current variant.
/// @return variant or NULL.
GVariant *svdb_item_get_variant(const SvdbTableItem *item);

/// @brief Get item type
/// @param item - current item.
/// @return current item type.
SvdbItemType svdb_item_get_type(const SvdbTableItem *item);

/// @brief Dump current item into pretty string.
/// @param item - current item.
/// @param path - table current path(or NULL)(Must begin and end with '/', or be "/")(no validations).
/// @param valueMode - true => dump like it value, false => dump like table(if item type is table).
/// @return Pretty output string.
GString *svdb_item_dump(const SvdbTableItem *item, const gchar *path, gboolean valueMode);

/// @brief Get item from list by name.
/// @param list - current list.
/// @param key - element path.
/// @return child SvdbTableItem (free with svdb_item_unref) or NULL.
SvdbTableItem *svdb_item_list_get_element(const SvdbTableItem *list, const gchar* key);

/// @brief Increase refcounter for current item.
/// @param item - current item.
/// @return current item.
SvdbTableItem *svdb_item_ref(const SvdbTableItem *item);

/// @brief Decrease refcounter for current item.
/// @param item - current item.
void svdb_item_unref(SvdbTableItem *item);

/// @brief Get SvdbTableItem in table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start with '/'. `is_dir == TRUE` => end with '/', else doesn't end with '/').
/// @param is_dir - if path is dir (end with '/') (return SVDB_TYPE_TABLE).
/// @param error - set value to error, if error occured.
/// @return if successful and `is_dir == TRUE`(`FALSE`), return child table (list/variant) by path.
SvdbTableItem* svdb_table_join_to(const SvdbTableItem* table, const gchar* path, gboolean is_dir, GError** error);

/// @brief Dump table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start and end with '/').
/// @param error - set value to error, if error occurred.
/// @return dump of table by path, or NULL.
GString *svdb_dump_path(const SvdbTableItem *table, const gchar *path, GError **error);

/// @brief List child items of table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start and end with '/').
/// @param error - set value to error, if error occurred.
/// @return dump of child table elements, or NULL.
GString *svdb_list_path(const SvdbTableItem *table, const gchar *path, GError **error);

/// @brief Dump variant value in table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start and end with '/').
/// @param error - set value to error, if error occurred.
/// @return dump of variant value, or NULL.
GString *svdb_read_path(const SvdbTableItem *table, const gchar *path, GError **error);

#endif // LIBSVDB_PRIVATE_GVDB
