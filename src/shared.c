#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <telepathy-glib/telepathy-glib.h>

#include "shared.h"

GMainLoop *loop = NULL;

static void
_list_connections_cb (const gchar * const *names,
                      gsize                n_names,
                      const gchar * const *cms,
                      const gchar * const *proto,
                      const GError        *in_error,
                      gpointer             user_data,
                      GObject             *weak_object)
{
    (void)cms; (void) proto; (void) weak_object;
    GTask *task = G_TASK (user_data);
    TpSimpleClientFactory *client;

    if (in_error) {
        g_task_return_error (task, g_error_copy (in_error));
        return;
    }

    client = (TpSimpleClientFactory*)g_task_get_task_data (task);

    GPtrArray *result = g_ptr_array_new ();
    for (unsigned int i = 0; i < n_names; i++) {
        gchar *path = g_strdelimit (
            g_strdup_printf ("/%s", names[i]), ".", '/');

        GError *error = NULL;
        TpConnection *connection = tp_simple_client_factory_ensure_connection(
            client,
            path,
            NULL,
            &error);

        g_ptr_array_add (result, connection);
        g_free (path);
    }

    g_task_return_pointer (task, (gpointer) result, NULL);
}

void
list_connections_async (TpSimpleClientFactory *client,
                        GAsyncReadyCallback    callback,
                        gpointer               user_data)
{
    GTask *task = g_task_new (client, NULL, callback, user_data);

    g_task_set_task_data (task, client, NULL);
    tp_list_connection_names (
        tp_simple_client_factory_get_dbus_daemon (client),
        _list_connections_cb,
        task,
        NULL, NULL);
}

gboolean
list_connections_finish (GObject       *source,
                         GAsyncResult  *result,
                         GPtrArray    **connections,
                         GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), FALSE);

    *connections = g_task_propagate_pointer (G_TASK (result), error);
    return TRUE;
}

static void
_list_channels_cb (TpProxy      *proxy,
                   const GValue *value,
                   const GError *in_error,
                   gpointer      user_data,
                   GObject      *weak_obj)
{
    (void) proxy; (void) weak_obj;
    GTask *task = G_TASK (user_data);
    TpConnection *connection;

    if (in_error) {
        g_task_return_error (task, g_error_copy (in_error));
        return;
    }

    connection = (TpConnection*)g_task_get_task_data (task);

    GPtrArray *channels = g_value_get_boxed (value);
    GPtrArray *result = g_ptr_array_new ();
    for (guint i = 0; i < channels->len; i++) {
        GValueArray *data = g_ptr_array_index (channels, i);
        char *object_path;
        GHashTable *map;

        tp_value_array_unpack (data, 2, &object_path, &map);

        GError *error = NULL;
        TpChannel *channel = tp_simple_client_factory_ensure_channel(
            tp_proxy_get_factory (connection),
            connection,
            object_path,
            map,
            &error);

        g_ptr_array_add (result, channel);
    }

    g_task_return_pointer (task, (gpointer) result, NULL);
}

void
list_channels_async (TpConnection        *connection,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    GTask *task = g_task_new (connection, NULL, callback, user_data);

    g_task_set_task_data (task, connection, NULL);
    tp_cli_dbus_properties_call_get (
        connection,
        -1,
        TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
        "Channels",
        _list_channels_cb,
        task,
        NULL, NULL);
}

gboolean
list_channels_finish (GObject       *source,
                      GAsyncResult  *result,
                      GPtrArray    **channels,
                      GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), FALSE);

    *channels = g_task_propagate_pointer (G_TASK (result), error);
    return TRUE;
}
