#include <svdb.h>
#include <svdb.h>
#include <stdio.h>


int main () {
    GString *tmp;
    SvdbTableItem *table;
    gchar *path;
    GError *error = NULL;
    if (g_file_test("../test/data/", G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
        path = "../test/data/";
    }
    else if(g_file_test("../../libsvdb/test/data/", G_FILE_TEST_IS_DIR | G_FILE_TEST_EXISTS)) {
        path = "../../libsvdb/test/data/";
    }
    else {
        g_error("%s", "test data folder doesn't found!");
    }

    path = g_strdup_printf("%s%s", path, "test_data1.gvdb");
    table = svdb_table_read_from_file(path, FALSE, &error);
    g_free(path);
    g_assert_no_error(error);

    tmp = svdb_dump_path(table, "/hash_table_2/", &error);
    g_assert_no_error(error);
    printf("%s", tmp ? tmp->str : "");
    tmp ? g_string_free(tmp, TRUE) : ("");

    tmp = svdb_list_path(table, "/hash_table_2/", &error);
    g_assert_no_error(error);
    printf("\n%s", tmp ? tmp->str : "");
    tmp ? g_string_free(tmp, TRUE) : ("");

    tmp = svdb_read_path(table, "/hash_table_2/v2_f64_1", &error);
    g_assert_no_error(error);
    printf("\n%s\n", tmp ? tmp->str : "");
    tmp ? g_string_free(tmp, TRUE) : ("");

}