#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "visitor.h"

GMainLoop *loop;

static char verbose;
static char acknowledge;
static char list_messages;
static char send_message;
static char single_target;
static char **users;

struct MessageWriter {
    char first;
} writer = {1};

#define BUF_SIZE 1024
static char msg_buffer[BUF_SIZE];

/* commandline arguments */
const
GOptionEntry entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        "Print more messages than just errors.", NULL },
    { "ack", 'a', 0, G_OPTION_ARG_NONE,  &acknowledge,
        "Acknowledge pending messages.", NULL },
    { "who", 'w', 0, G_OPTION_ARG_NONE, &list_messages,
        "List open channels", NULL },
    { "send", 's', 0, G_OPTION_ARG_NONE, &send_message,
        "Send a message", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &users,
        "USER", NULL },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
};

static gboolean
contact_status_unknown (TpContact *contact)
{
    return strcmp (tp_contact_get_presence_status (contact), "unknown") != 0;
}

static gboolean
single_element (char **array)
{
    return array && array[0] && !array[1];
}

static void
message_cb (TpMessage *message)
{
    TpContact *sender = tp_signalled_message_get_sender (message);
    GDateTime *received = g_date_time_new_from_unix_utc (
        tp_message_get_received_timestamp (message));
    gchar *timestamp = g_date_time_format (received, "%Y-%m-%d %H:%M:%S");
    const gchar *sender_identifier = tp_contact_get_identifier (sender);
    const gchar *sender_alias = tp_contact_get_alias (sender);

    if (!writer.first) {
        g_print ("\n\n");
    } else {
        writer.first = 0;
    }

    printf ("Message-Token: %s\n", tp_message_get_token (message));
    printf ("From: \"%s\" <%s>\n", sender_alias, sender_identifier);
    printf ("Date: %s\n\n", timestamp);

    g_free (timestamp);

    unsigned int parts = tp_message_count_parts (message);
    for (unsigned int i = 1; i < parts; i++) {
        const GHashTable *part = tp_message_peek (message, i);
        const gchar *content_type = g_value_get_string (
            g_hash_table_lookup ((GHashTable*)part, "content-type"));
        const gchar *content = g_value_get_string (
            g_hash_table_lookup ((GHashTable*)part, "content"));

        if (verbose) {
            printf ("- #%d %s\n", i, content_type);
        }
        printf ("%s\n", content);
    }
}

static void
contact_cb (ChVisitor *visitor,
            TpContact *contact)
{
    (void) visitor;

    if (list_messages) {
        printf ("%s\t%s\n",
                tp_contact_get_alias (contact),
                tp_contact_get_identifier (contact));
    }

    if (send_message) {
        if (contact_status_unknown (contact)) {
            ch_visitor_visit_contact_channel (visitor, contact);
        }
    }
}

static void
ack_messages_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    (void) user_data;

    GError *error = NULL;
    tp_text_channel_ack_messages_finish (TP_TEXT_CHANNEL (source),
                                         result,
                                         &error);
}

void
send_message_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    ChVisitor *visitor = (ChVisitor*) user_data;
    TpTextChannel *channel = TP_TEXT_CHANNEL (source);
    gchar *token;
    GError *error;

    if (!tp_text_channel_send_message_finish (channel, result, &token, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    if (verbose) {
        g_printerr ("message sent %s", token);
    }

    ch_visitor_decref (visitor);
}

static void
channel_ready (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    ChVisitor *visitor = (ChVisitor*) user_data;
    TpChannel *channel;
    TpMessage *message;

    GError *error = NULL;
    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    channel = TP_CHANNEL (source);

    if (verbose > 0) {
        g_printerr ("channel_ready \"%s\" (type %s)\n",
                    tp_channel_get_identifier (channel),
                    tp_channel_get_channel_type (channel));
    }

    GList *messages, *iter;
    if (TP_IS_TEXT_CHANNEL (channel)) {
        messages = tp_text_channel_dup_pending_messages (
             TP_TEXT_CHANNEL (channel));

        for (iter = messages; iter; iter = iter->next) {
            TpMessage *message = TP_MESSAGE (iter->data);
            message_cb (message);
        }

        if (acknowledge) {
            tp_text_channel_ack_messages_async (TP_TEXT_CHANNEL (channel),
                                                messages,
                                                ack_messages_cb,
                                                NULL);
        }

        g_list_free_full (messages, g_object_unref);

        if (send_message) {
            message = tp_client_message_new_text(0, msg_buffer);
            ch_visitor_incref (visitor);
            tp_text_channel_send_message_async (TP_TEXT_CHANNEL (channel),
                                                message,
                                                0,
                                                send_message_cb,
                                                visitor);
        }
    } else {
        g_printerr ("error: %s is not a text channel\n",
                    tp_channel_get_identifier (channel));
    }

    ch_visitor_decref (visitor);
}

static void
channel_cb (ChVisitor *visitor,
            TpChannel *channel)
{
    const char *type = tp_channel_get_channel_type (channel);
    const char *ident = tp_channel_get_identifier (channel);
    char **userlist;

    // if this is a text channel probe it for pending messages
    if (!strcmp (type, TP_IFACE_CHANNEL_TYPE_TEXT))
    {
        // filter to only include given channels
        if ((userlist = users)) {
            for (; *userlist; userlist += 1) {
                if (!strcmp (ident, *userlist)) {
                    break;
                }
            }
            if (!*userlist) {
                return;
            }
        }

        if (list_messages) {
            ch_visitor_visit_channel_target (visitor, channel);
            return;
        }

        GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0};

        ch_visitor_incref (visitor);
        tp_proxy_prepare_async (channel,
                                features,
                                channel_ready,
                                visitor);
    } else {
        if (verbose > 0) {
            g_printerr ("ignored channel %s %s\n", ident, type);
        }
    }
}

static void
connection_cb (ChVisitor    *visitor,
               TpConnection *connection,
               guint         status)
{
    (void) visitor;
    char **userit;

    TpContact *self;
    if (status == 0) {
        if (verbose > 0) {
            self = tp_connection_get_self_contact (connection);

            g_printerr ("connection ready: %s/%s (%s)\n",
                        tp_connection_get_cm_name (connection),
                        tp_connection_get_protocol_name(connection),
                        self ? tp_contact_get_identifier (self) : "n/a");
        }

        if (send_message) {
            for (userit = users; *userit; userit += 1) {
                ch_visitor_visit_connection_contact (visitor,
                                                     connection,
                                                     *userit);
            }
        } else {
            ch_visitor_visit_channels (visitor, connection);
        }
    }
}

static void
dispose_cb (ChVisitor *visitor)
{
    (void) visitor;

    g_main_loop_quit (loop);
}

static void
read_message ()
{
    fread (msg_buffer, BUF_SIZE, 1, stdin);
}

int
main (int argc, char **argv)
{
    ChVisitor *visitor;
    TpSimpleClientFactory *factory = NULL;

    GQuark contact_features[] = { TP_CONTACT_FEATURE_ALIAS,
                                  TP_CONTACT_FEATURE_PRESENCE,
                                  0 };

    /* Parse commandline arguments */
    GOptionContext* context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free (context);

    single_target = single_element (users);

    if (single_target) {
        send_message = TRUE;
    }

    if (send_message) {
        read_message ();
    }

    loop = g_main_loop_new (NULL, FALSE);

    factory = (TpSimpleClientFactory*) tp_automatic_client_factory_new (NULL);

    tp_simple_client_factory_add_contact_features (factory,
                                                   3,
                                                   contact_features);

    visitor = ch_visitor_new (factory);
    visitor->visit_channel = channel_cb;
    visitor->visit_connection = connection_cb;
    visitor->visit_contact = contact_cb;
    visitor->dispose = dispose_cb;
    ch_visitor_visit_connections (visitor);

    g_main_loop_run (loop);

    g_object_unref (factory);

    return 0;
}
