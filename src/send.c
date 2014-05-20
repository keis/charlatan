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
channel_cb (ChVisitor *visitor,
            TpChannel *channel)
{
    TpAccount *account;
    TpConnection *connection;
    TpContact *contact;
    TpClientMessage *message;

    connection = tp_channel_get_connection (channel);
    account = tp_connection_get_account (connection);
    contact = tp_channel_get_target_contact (channel);

    printf("hello %s from %s\n",
           tp_channel_get_identifier (channel),
           tp_proxy_get_object_path (account));
    printf("contact %s %d\n",
           tp_contact_get_identifier (contact),
           tp_contact_get_subscribe_state (contact));

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

        ch_visitor_visit_contact_channel (visitor, contact);
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
    visitor->visit_channel = channel_cb;
    visitor->dispose = dispose_cb;
    ch_visitor_visit_connections (visitor);

    g_main_loop_run (loop);

    g_object_unref (factory);

    return 0;
}
