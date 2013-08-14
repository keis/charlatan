#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <telepathy-glib/telepathy-glib.h>

#include "shared.h"

ForEachChannelCb for_each_channel_cb = NULL;
ForEachConnectionCb for_each_connection_cb = NULL;
GMainLoop *loop = NULL;
unsigned int pending = 0;

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
get_channels_cb (TpProxy      *proxy,
                 const GValue *value,
                 const GError *in_error,
                 gpointer      user_data,
                 GObject      *weak_obj)
{
    (void) proxy; (void) weak_obj;

    if (in_error) {
        g_printerr ("error: %s\n", in_error->message);
    }

    TpConnection *connection = (TpConnection*) user_data;

    g_return_if_fail (
        G_VALUE_HOLDS (value, TP_ARRAY_TYPE_CHANNEL_DETAILS_LIST));

    GPtrArray *channels = g_value_get_boxed (value);
    for (guint i = 0; i < channels->len; i++) {
        GValueArray *channel = g_ptr_array_index (channels, i);
        char *object_path;
        GHashTable *map;

        tp_value_array_unpack (channel, 2, &object_path, &map);

        for_each_channel_cb (connection, object_path, map);
    }

    pending -= 1;
    if (pending == 0) {
        g_main_loop_quit (loop);
    }
}

static void
connection_ready (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    (void) user_data;

    GError *error = NULL;
    TpConnection *connection = TP_CONNECTION (source);

    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
    }

    // request the current channels if needed
    if (for_each_channel_cb) {
        tp_cli_dbus_properties_call_get (
                connection, -1,
                TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
                "Channels",
                get_channels_cb,
                (gpointer)connection, NULL, NULL);
    } else {
        pending -= 1;
        if (pending == 0) {
            g_main_loop_quit (loop);
        }
    }

    // Run callback, 0 indicates failure reason
    if (for_each_connection_cb)
        for_each_connection_cb (connection, 0);
}

static void
connection_status (TpConnection *connection,
                   guint         status,
                   const GError *in_error,
                   gpointer      user_data,
                   GObject      *weak_object)
{
    (void) user_data; (void) weak_object;

    if (in_error) {
        g_printerr ("error: %s\n", in_error->message);
    }

    // Run callback for inactive connections
    if (status != 0) {
        if (for_each_connection_cb)
            for_each_connection_cb (connection, status);

        pending -= 1;
        if (pending == 0) {
            g_main_loop_quit (loop);
        }
    }
}

static void
connection_list_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    (void) user_data;

    GError *error = NULL;
    GPtrArray *connections;

    if (!list_connections_finish (source, result, &connections, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    for (unsigned int i = 0; i < connections->len; i++)
    {
        TpConnection *connection = g_ptr_array_index (connections, i);

        pending += 1;
        tp_proxy_prepare_async (connection, NULL, connection_ready, NULL);
        tp_cli_connection_call_get_status (
            connection, -1,
            connection_status, NULL, NULL, NULL);
    }

    g_ptr_array_free (connections, TRUE);
}

void
tpic_run (TpSimpleClientFactory *client)
{
    if (for_each_channel_cb || for_each_connection_cb) {
        list_connections_async (client, connection_list_cb, NULL);
    }
}
