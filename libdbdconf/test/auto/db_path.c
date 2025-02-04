#include <libdbdconf/utils.h>
#include <stdio.h>

void test_db_path1() {
    DBPath* path1 = dbpath_new_from_path("/");
    DBPath* path2 = dbpath_new_from_path("/test");
    DBPath* path3 = dbpath_new_from_path("test");
    DBPath* path4 = dbpath_new_from_path("/test/test2");
    DBPath* path5 = dbpath_new_from_path("/test/test2/");

    GString* path_string1 = dbpath_to_string(path1);
    GString* path_string2 = dbpath_to_string(path2);
    GString* path_string3 = dbpath_to_string(path3);
    GString* path_string4 = dbpath_to_string(path4);
    GString* path_string5 = dbpath_to_string(path5);

    g_assert(g_str_equal(path_string1->str, "/"));
    g_assert(g_str_equal(path_string2->str, "/test/"));
    g_assert(g_str_equal(path_string3->str, "/test/"));
    g_assert(g_str_equal(path_string4->str, "/test/test2/"));
    g_assert(g_str_equal(path_string5->str, "/test/test2/"));
}

int main() {
    test_db_path1();
}
