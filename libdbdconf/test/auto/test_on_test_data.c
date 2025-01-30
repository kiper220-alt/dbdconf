#include <libdbdconf/gvdb.h>
#include <stdio.h>
#include <gvdb-reader.h>

int main() {
    GVariantTableItem* dbd_table = dbd_table_read_from_file("./test_data.gvdb", FALSE, NULL);
    GString* dump = dbd_table_dump(dbd_table, NULL);
    printf("%s\n", dump->str);
    g_hash_table_unref(dbd_table);

    g_free(dump);
    GvdbTable* gvdb_table = gvdb_table_new("./test_data.gvdb", FALSE, NULL);
    gvdb_table_free(gvdb_table);
}
