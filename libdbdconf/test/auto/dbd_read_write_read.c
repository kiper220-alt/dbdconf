#include <libdbdconf/gvdb.h>
#include <stdio.h>

// test_data1 - TEST FAILED.
// test_data2 - TEST FAILED.
// test_data3 - TEST PASSED.
// test_data4 - TEST PASSED.

// with deduction method, error in LIST writer.

int main() {
    GDir *dir;
    GString *tmp;
    GVariantTableItem *table;
    const gchar *path;
    const gchar *filename;
    GError *error = NULL;
    GBytes* bytes;

    if (g_file_test("../test/data/", G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
        path = "../test/data/";
    }
    else if(g_file_test("../../libdbdconf/test/data/", G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
        path = "../../libdbdconf/test/data/";
    }
    else {
        g_error("%s", "test data folder doesn't found!");
    }

    dir = g_dir_open(path, 0, &error);
    g_assert_no_error(error);

    while ((filename = g_dir_read_name(dir))) {
        filename = g_strdup_printf("%s/%s", path, filename);
        table = dbd_table_read_from_file(filename, FALSE, &error);
        g_assert_no_error(error);

        bytes = dbd_table_get_raw(table, FALSE, &error);
        g_assert_no_error(error);
        dbd_item_unref(table);

        table = dbd_table_read_from_bytes(bytes, FALSE, &error);
        dbd_item_unref(table);

        g_assert_no_error(error);
        g_bytes_unref(bytes);
        g_free((gpointer) filename);
    }
    g_dir_close(dir);
}
