#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "shared.h"

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
    TpConnection *connection;
    TpAccount *account;
    TpAccountChannelRequest *req;

    contact = tp_simple_client_factory_ensure_contact_by_id_finish (source, result, &error);

    if (error) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    if (strcmp (tp_contact_get_presence_status (contact), "unknown") != 0) {
        connection = tp_contact_get_connection (contact);
        account = tp_connection_get_account (connection);

        req = tp_account_channel_request_new_text (account,
                                                   0);

        tp_account_channel_request_set_target_contact (req,
                                                       contact);

        tp_account_channel_request_ensure_and_handle_channel_async (
            req,
            NULL,
            ensure_channel_cb,
            account);
    }
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
list_connections_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    (void) user_data;

    GError *error = NULL;
    GPtrArray *connections;
    TpConnection *connection;
    GQuark connection_features[] = { TP_CONNECTION_FEATURE_CONNECTED,
                                     0 };

    if (!list_connections_finish (source, result, &connections, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    for (unsigned int i = 0; i < connections->len; i++) {
        connection = TP_CONNECTION (g_ptr_array_index (connections, i));

        tp_proxy_prepare_async (connection,
                                connection_features,
                                connection_ready,
                                NULL);
    }
}

int
main (int argc, char **argv)
{
    TpAccountManager *accman;
    TpSimpleClientFactory *factory;

    GQuark contact_features[] = { TP_CONTACT_FEATURE_ALIAS,
                                  TP_CONTACT_FEATURE_PRESENCE,
                                  0 };

    loop = g_main_loop_new (NULL, FALSE);

    factory = (TpSimpleClientFactory*) tp_automatic_client_factory_new (NULL);

    tp_simple_client_factory_add_contact_features (factory,
                                                   3,
                                                   contact_features);

    accman = tp_account_manager_new_with_factory (factory);


    list_connections_async (accman, list_connections_cb, NULL);

    g_main_loop_run (loop);
    return 0;
}
