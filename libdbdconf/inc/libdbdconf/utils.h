#include <glib.h>

typedef struct DBPath_t {
    GString** element;
    gsize length;
} DBPath;

DBPath* dbpath_new();
DBPath* dbpath_copy(DBPath* path);
DBPath* dbpath_free(DBPath* path);
DBPath* dbpath_new_from_path(const gchar* path);
gboolean dbpath_pop(DBPath* path);
void dbpath_push(DBPath* path, const gchar* element);
void dbpath_push_len(DBPath* path, const gchar* element, gsize len);
GString* dbpath_to_string(DBPath* path);

