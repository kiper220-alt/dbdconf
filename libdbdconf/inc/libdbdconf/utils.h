#include <glib.h>
#include <libdbdconf/gvdb.h>

/// @brief Get GVariantTableItem in table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start with '/'. `is_dir == TRUE` => end with '/', else doesn't end with '/').
/// @param is_dir - if path is dir (end with '/') (return DBD_TYPE_TABLE).
/// @param error - set value to error, if error occured.
/// @return if successful and `is_dir == TRUE`(`FALSE`), return child table (list/variant) by path.
GVariantTableItem* dbd_table_join_to(GVariantTableItem* table, const gchar* path, gboolean is_dir, GError** error);
/// @brief Dump table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start and end with '/').
/// @param error - set value to error, if error occurred.
/// @return dump of table by path, or NULL.
GString* dbd_dump_path(GVariantTableItem* table, const gchar* path, GError** error);
/// @brief List child items of table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start and end with '/').
/// @param error - set value to error, if error occurred.
/// @return dump of child table elements, or NULL.
GString* dbd_list_path(GVariantTableItem* table, const gchar* path, GError** error);
/// @brief Dump variant value in table by path.
/// @param table - root table for path context.
/// @param path - path in root table (must start and end with '/').
/// @param error - set value to error, if error occurred.
/// @return dump of variant value, or NULL.
GString* dbd_read_path(GVariantTableItem* table, const gchar* path, GError** error);

