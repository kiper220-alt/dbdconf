#include <libdbdconf/gvdb.h>
#include <gvdb-reader.h>
#include <gvdb-builder.h>

gboolean compare_string_lists(gchar** list1, gchar** list2) {
    while (*list1 && *list2) {
        if (strcmp(*list1, *list2) != 0) {
            return FALSE;
        }
        ++list1, ++list2;
    }
    if (*list1 != *list2) {
        return FALSE;
    }

    return TRUE;
}

void build_gvdb(const gchar* filename, gboolean byteswap, GError **error) {
    GError* tmp = NULL;
    GHashTable *root = gvdb_hash_table_new(NULL, NULL);
    GHashTable *hash_table = gvdb_hash_table_new(root, "hash_table");
    GvdbItem *array = gvdb_hash_table_insert(root, "list1");
    GvdbItem *item;
    item = gvdb_hash_table_insert(root, "list11");
    gvdb_item_set_value(item, g_variant_new_string("value11"));
    gvdb_item_set_parent(item, array);
    item = gvdb_hash_table_insert(root, "list12");
    gvdb_item_set_value(item, g_variant_new_string("value12"));
    gvdb_item_set_parent(item, array);
    item = gvdb_hash_table_insert(root, "list13");
    gvdb_item_set_value(item, g_variant_new_string("value13"));
    gvdb_item_set_parent(item, array);
    item = gvdb_hash_table_insert(root, "list14");
    gvdb_item_set_value(item, g_variant_new_string("value14"));
    gvdb_item_set_parent(item, array);

    array = gvdb_hash_table_insert(root, "list2");
    item = gvdb_hash_table_insert(root, "list21");
    gvdb_item_set_value(item, g_variant_new_string("value21"));
    gvdb_item_set_parent(item, array);
    item = gvdb_hash_table_insert(root, "list22");
    gvdb_item_set_value(item, g_variant_new_string("value22"));
    gvdb_item_set_parent(item, array);
    item = gvdb_hash_table_insert(root, "list23");
    gvdb_item_set_value(item, g_variant_new_string("value23"));
    gvdb_item_set_parent(item, array);
    item = gvdb_hash_table_insert(root, "list24");
    gvdb_item_set_value(item, g_variant_new_string("value24"));
    gvdb_item_set_parent(item, array);

    item = gvdb_hash_table_insert(root, "string");
    gvdb_item_set_value(item, g_variant_new_string("string_data"));
    
    item = gvdb_hash_table_insert(root, "integer");
    gvdb_item_set_value(item, g_variant_new_int32(1234567890));

    gvdb_hash_table_insert_string(hash_table, "hash_string", "hash_value");

    gvdb_table_write_contents(root, filename, byteswap, &tmp);

    if (tmp) {
        if (error) {
            *error = tmp;
        }
        return;
    }
}

void read_gvdb(const gchar* filename, gboolean trusted, GError **error) {
    GError *tmp = NULL;
    GvdbTable *table = gvdb_table_new(filename, trusted, &tmp);
    GvdbTable *hash_table = gvdb_table_get_table(table, "hash_table");

    if (tmp) {
        if (error) {
            *error = tmp;
        }
        return;
    }
    gchar** list_1 = gvdb_table_list(table, "list1");
    gchar* orig_list_1[] = {
        "1",
        "2",
        "3",
        "4",
        NULL
    };

    g_assert(compare_string_lists(list_1, orig_list_1));

    g_strfreev(list_1);
    list_1 = gvdb_table_list(table, "list2");
    
    g_assert(compare_string_lists(list_1, orig_list_1));
    GVariant *variant; 
    const gchar* string;
    
    // clang-format off
    variant = gvdb_table_get_value(table, "list11"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value11") == 0); g_variant_unref(variant);
    variant = gvdb_table_get_value(table, "list12"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value12") == 0); g_variant_unref(variant);
    variant = gvdb_table_get_value(table, "list13"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value13") == 0); g_variant_unref(variant);
    variant = gvdb_table_get_value(table, "list14"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value14") == 0); g_variant_unref(variant);

    variant = gvdb_table_get_value(table, "list21"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value21") == 0); g_variant_unref(variant);
    variant = gvdb_table_get_value(table, "list22"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value22") == 0); g_variant_unref(variant);
    variant = gvdb_table_get_value(table, "list23"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value23") == 0); g_variant_unref(variant);
    variant = gvdb_table_get_value(table, "list24"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "value24") == 0); g_variant_unref(variant);

    variant = gvdb_table_get_value(table, "string"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "string_data") == 0); g_variant_unref(variant);

    variant = gvdb_table_get_value(table, "integer"); g_assert(g_variant_get_int32(variant) == 1234567890); g_variant_unref(variant);

    variant = gvdb_table_get_value(hash_table, "hash_string"); string = g_variant_get_string(variant, NULL); g_assert(strcmp(string, "hash_value") == 0); g_variant_unref(variant);
    // clang-format on 

}

void read_write_dbd(const gchar* source, const gchar* target) {
    GError *error = NULL;
    GVariantTableItem* table = dbd_table_read_from_file("./readwrite_test.gvdb", FALSE, &error);
    g_assert_no_error(error);

    dbd_table_write_to_file(table, "./readwrite_test2.gvdb", FALSE, &error);
    g_assert_no_error(error);
}

int main() {
    GError* error = NULL;
    
    build_gvdb("./readwrite_test.gvdb", FALSE, NULL);
    g_assert_no_error(error);
    
    read_gvdb("./readwrite_test.gvdb", FALSE, NULL);
    g_assert_no_error(error);
    
    read_write_dbd("./readwrite_test.gvdb", "./readwrite_test2.gvdb");

    read_gvdb("./readwrite_test2.gvdb", FALSE, NULL);
    g_assert_no_error(error);
}
