#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "shared.h"
#include "visitor.h"

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

static void
contact_cb (ChVisitor *visitor,
            TpContact *contact)
{
    TpAccount *account;
    TpConnection *connection;
    TpAccountChannelRequest *req;

    if (strcmp (tp_contact_get_presence_status (contact), "unknown") != 0) {
        connection = tp_contact_get_connection (contact);
        account = tp_connection_get_account (connection);

        ch_visitor_incref (visitor);
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

static void
connection_cb (ChVisitor    *visitor,
               TpConnection *connection,
               guint         status)
{
    (void) visitor;

    if (status == 0) {
        ch_visitor_visit_connection_contact (visitor,
                                             connection,
                                             "christer.gustavsson@klarna.com");
    }
}

static void
dispose_cb (ChVisitor *visitor)
{
    (void) visitor;

    g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
    ChVisitor *visitor;
    TpSimpleClientFactory *factory;

    GQuark contact_features[] = { TP_CONTACT_FEATURE_ALIAS,
                                  TP_CONTACT_FEATURE_PRESENCE,
                                  0 };

    loop = g_main_loop_new (NULL, FALSE);

    factory = (TpSimpleClientFactory*) tp_automatic_client_factory_new (NULL);

    tp_simple_client_factory_add_contact_features (factory,
                                                   3,
                                                   contact_features);

    visitor = ch_visitor_new (factory);
    visitor->visit_connection = connection_cb;
    visitor->visit_contact = contact_cb;
    visitor->dispose = dispose_cb;
    ch_visitor_visit_connections (visitor);

    g_main_loop_run (loop);

    g_object_unref (factory);

    return 0;
}
