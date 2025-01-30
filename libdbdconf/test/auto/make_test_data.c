#include <gvdb-builder.h>

int main()
{
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


    // GHashTable* child = gvdb_hash_table_new(root, "some_key2");
    // gvdb_item_set_value(gvdb_hash_table_insert(child, "some_key3"), g_variant_new_int16(6));

    gvdb_table_write_contents(root, "./test_data.gvdb", FALSE, NULL);
}
