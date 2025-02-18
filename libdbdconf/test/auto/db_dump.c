#include <libdbdconf/gvdb.h>
#include <stdio.h>

void dump_table(const gchar* file_path) {
    g_assert(file_path && *file_path);
    GError *error = NULL;
    GVariantTableItem *table = dbd_table_read_from_file(file_path, FALSE, &error);
    g_assert_no_error(error);

    GString* dump = dbd_table_dump(table, "/");
    g_assert(dump);

    printf("%s", dump->str);
    g_string_free(dump, TRUE);
    dbd_item_unref(table);
}

int main() {
    GDir *dir;
    GError *error = NULL;
    const gchar *filename;
    const gchar *path;

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
        gchar* file_full_path = g_strdup_printf("%s%s", path, filename);
        dump_table(file_full_path);
        g_free(file_full_path);
    }

    g_dir_close(dir);
}
