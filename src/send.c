#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

GMainLoop *loop;

void
message_sent (GObject      *source,
              GAsyncResult *result,
              gpointer      user_data)
{
    gchar *token;
    GError *error;

    if (!tp_text_channel_send_message_finish (source, result, &token, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    printf("sent %s", token);

    g_main_loop_quit (loop);
}

void
ensure_channel_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    TpAccount *account = (TpAccount*) user_data;
    GPtrArray *channels;
    TpChannel *chan;
    TpClientMessage *message;
    GError *error = NULL;
    TpHandleChannelsContext *ctx;

    if (!tp_account_channel_request_ensure_and_handle_channel_finish (source, result, &ctx, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    g_object_get (ctx, "channels", &channels, NULL);
    chan = g_ptr_array_index (channels, 0);
    printf("hello %s from %s\n",
           tp_channel_get_identifier (chan),
           tp_proxy_get_object_path (account));
    TpContact *con = tp_channel_get_target_contact (chan);
    printf("contact %s %d\n",
           tp_contact_get_identifier (con),
           tp_contact_get_subscribe_state (con));

    //message = tp_client_message_new_text(0, "hej");
    //tp_text_channel_send_message_async (chan, message, 0, message_sent, NULL);
}

void
ensure_contact_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    GError *error = NULL;
    TpContact *contact;

    contact = tp_simple_client_factory_ensure_contact_by_id_finish (source, result, &error);

    if (error) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    printf("YO %s\n", tp_contact_get_presence_status (contact));
}

void
connection_ready  (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    GError *error = NULL;
    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    TpConnection *connection = TP_CONNECTION (source);

    tp_simple_client_factory_ensure_contact_by_id_async (
        tp_proxy_get_factory (connection),
        connection,
        "christer.gustavsson@klarna.com",
        ensure_contact_cb,
        NULL);
}

void
list_accounts (TpAccountManager *manager, TpSimpleClientFactory *client)
{
    GList *accounts = tp_account_manager_dup_valid_accounts (manager);
    GList *it;
    TpAccount *account;

    GQuark connection_features[] = { TP_CONNECTION_FEATURE_CONNECTED,
                                     0 };

    for (it = accounts; it; it = it->next) {
        account = it->data;
        printf("account %s\n", tp_proxy_get_object_path (account));

        if (!tp_account_is_enabled (account)) {
            continue;
        }

        tp_proxy_prepare_async (tp_account_get_connection (account),
                                connection_features,
                                connection_ready,
                                NULL);

        /*TpAccountChannelRequest *req = tp_account_channel_request_new_text (
            account,
            0);

        tp_account_channel_request_set_target_id (
            req,
            TP_HANDLE_TYPE_CONTACT,
            "christer.gustavsson@klarna.com");

        tp_account_channel_request_ensure_and_handle_channel_async (
            req,
            NULL,
            ensure_channel_cb,
            account);*/
    }

    g_list_free_full (accounts, g_object_unref);
}


static void
account_manager_ready (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
    TpSimpleClientFactory *client = (TpSimpleClientFactory*) user_data;

    GError *error = NULL;
    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    TpAccountManager *manager = TP_ACCOUNT_MANAGER (source);
    list_accounts (manager, client);
}

int
main (int argc, char **argv)
{
    TpChannelDispatcher *dispatcher;
    TpAccountManager *accman;
    TpSimpleClientFactory *factory;
    TpDBusDaemon *bus;
    GHashTable *request;
    gchar *account_path = argv[1];

    GQuark contact_features[] = { TP_CONTACT_FEATURE_ALIAS,
                                  TP_CONTACT_FEATURE_PRESENCE,
                                  0 };

    loop = g_main_loop_new (NULL, FALSE);

    factory = (TpSimpleClientFactory*) tp_automatic_client_factory_new (NULL);

    tp_simple_client_factory_add_contact_features (factory,
                                                   3,
                                                   contact_features);

    bus = tp_simple_client_factory_get_dbus_daemon (factory);
    dispatcher = tp_channel_dispatcher_new (bus);
    accman = tp_account_manager_new_with_factory (factory);


    tp_proxy_prepare_async (accman,
                            NULL,
                            account_manager_ready,
                            factory);

    g_main_loop_run (loop);
    return 0;
}
