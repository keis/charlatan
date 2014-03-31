#include <glib.h>
#include <glib-object.h>
#include "debug.h"

void
dump_hash_table (GHashTable *tbl)
{
    GHashTableIter iter;
    gchar* key;
    GValue *value;

    g_hash_table_iter_init (&iter, tbl);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value))
    {
        g_print("%s:%s\n", key, g_strdup_value_contents (value));
    }
}
