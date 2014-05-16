#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <telepathy-glib/telepathy-glib.h>

#include "shared.h"

static void
_list_connections_accounts_cb (GObject      *source,
                               GAsyncResult *result,
                               gpointer      user_data)
{
    GList *accounts, *it;
    GError *error = NULL;
    GTask *task = G_TASK (user_data);
    GPtrArray *connections;
    TpAccount *account;
    TpConnection *connection;

    if (!list_accounts_finish (source, result, &accounts, &error)) {
        g_task_return_error (task, g_error_copy (error));
        return;
    }

    connections = g_ptr_array_new ();
    for (it = accounts; it; it = it->next) {
        account = TP_ACCOUNT (it->data);
        connection = tp_account_get_connection (account);

        if (connection) {
            g_ptr_array_add (connections, connection);
        }
    }

    g_task_return_pointer (task, (gpointer) connections, NULL);
}

void
list_connections_async (TpAccountManager      *manager,
                        GAsyncReadyCallback    callback,
                        gpointer               user_data)
{
    GTask *task = g_task_new (manager, NULL, callback, user_data);

    list_accounts_async (manager, _list_connections_accounts_cb, task);
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

static void
list_accounts_manager_ready (GObject      *source,
                             GAsyncResult *result,
                             gpointer      user_data)
{
    TpAccountManager *manager;
    GError *error = NULL;
    GTask *task = G_TASK (user_data);
    GList *accounts;

    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_task_return_error (task, g_error_copy (error));
        return;
    }

    manager = g_task_get_source_object (task);
    accounts = tp_account_manager_dup_valid_accounts (manager);

    g_task_return_pointer (task, (gpointer) accounts, NULL);
}

void
list_accounts_async (TpAccountManager    *manager,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
    GTask *task = g_task_new (manager, NULL, callback, user_data);

    tp_proxy_prepare_async (manager,
                            NULL,
                            list_accounts_manager_ready,
                            task);
}

gboolean
list_accounts_finish (GObject       *source,
                      GAsyncResult  *result,
                      GList        **accounts,
                      GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, source), FALSE);

    *accounts = g_task_propagate_pointer (G_TASK (result), error);
    return TRUE;
}
