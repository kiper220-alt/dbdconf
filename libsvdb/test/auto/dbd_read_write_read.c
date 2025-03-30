#include <svdb.h>
#include <stdio.h>

int main() {
    GDir *dir;
    GString *tmp;
    SvdbTableItem *table;
    const gchar *path;
    const gchar *filename;
    GError *error = NULL;
    GBytes *bytes;

    if (g_file_test("../test/data/", G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
        path = "../test/data/";
    } else if (g_file_test("../../libsvdb/test/data/", G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
        path = "../../libsvdb/test/data/";
    } else {
        g_error("%s", "test data folder doesn't found!");
    }

    dir = g_dir_open(path, 0, &error);
    g_assert_no_error(error);

    while ((filename = g_dir_read_name(dir))) {
        filename = g_strdup_printf("%s/%s", path, filename);
        table = svdb_table_read_from_file(filename, FALSE, &error);
        g_assert_no_error(error);

        bytes = svdb_table_get_raw(table, FALSE, &error);
        g_assert_no_error(error);
        svdb_item_unref(table);

        table = svdb_table_read_from_bytes(bytes, FALSE, &error);
        svdb_item_unref(table);

        g_assert_no_error(error);
        g_bytes_unref(bytes);
        g_free((gpointer) filename);
    }
    g_dir_close(dir);
}
