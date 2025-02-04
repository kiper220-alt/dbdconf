#include <libdbdconf/utils.h>

DBPath* dbpath_new() {
    return g_malloc0(sizeof(DBPath));
}

DBPath* dbpath_copy(DBPath* path) {
    DBPath* copy = g_malloc0(sizeof(DBPath));
    copy->element = g_malloc(path->length);
    copy->length = path->length;
    
    for (gsize i = 0; i < path->length; ++i) {
        copy->element[i] = g_string_new_len(path->element[i]->str, path->element[i]->len);
    }

    return copy;
}
DBPath* dbpath_free(DBPath* path) {
    for (gsize i = path->length; i > 0; --i) {
        g_string_free(path->element[i - 1], TRUE);
    }
    g_free(path->element);
    g_free(path);
}

DBPath* dbpath_new_from_path(const gchar* path) {
    const gchar* cursor = path + 1;
    DBPath* db_path = dbpath_new();

    if (!path || path[0] == '\0' || path[0] == '/' && path[1] == '\0') {
        return db_path;
    }

    if (*path == '/') {
        ++path;
    }

    while(*path != '\0') {
        while (*cursor != '\0' && *cursor != '/') {
            ++cursor;
        }

        
        if (cursor == path + 1 && *path == '/') { // "...//..."
            return dbpath_new();
        }

        dbpath_push_len(db_path, path, cursor - path);
        
        if (*cursor == '\0') {
            break;
        }
        
        path = cursor + 1;
        cursor = path + 1;
    }

    return db_path;
}
gboolean dbpath_pop(DBPath* path) {
    if (path->length == 0) {
        return FALSE;
    }

    g_string_free(path->element[path->length - 1], TRUE);
    path->element = g_realloc_n(path->element, --path->length, sizeof(path->element));

    return TRUE;
}
void dbpath_push(DBPath* path, const gchar* element) {
    path->element = g_realloc_n(path->element, path->length + 1, sizeof(path->element));
    path->element[path->length++] = g_string_new(element);

    return TRUE;
}

GString* dbpath_to_string(DBPath* path) {
    if (!path || !path->length) {
        return g_string_new("/");
    }
    GString* string = g_string_new("/");
    g_string_append_len(string, path->element[0]->str, path->element[0]->len);

    for (int i = 1; i < path->length; ++i) {
        g_string_append_len(string, "/", 1);
        g_string_append_len(string, path->element[i]->str, path->element[i]->len);
    }
    g_string_append_len(string, "/", 1);

    return string;
}

void dbpath_push_len(DBPath* path, const gchar* element, gsize len) {
    path->element = g_realloc_n(path->element, path->length + 1, sizeof(path->element));
    path->element[path->length++] = g_string_new_len(element, len);

    return TRUE;
}
