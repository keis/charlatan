#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>
#include <time.h>

#include "shared.h"

static char verbose;
static char acknowledge;
static char list_messages;
static char **users;

/* commandline arguments */
const
GOptionEntry entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        "Print more messages than just errors.", NULL },
    { "ack", 'a', 0, G_OPTION_ARG_NONE,  &acknowledge,
        "Acknowledge pending messages.", NULL },
    { "who", 'w', 0, G_OPTION_ARG_NONE, &list_messages,
        "List open channels", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &users,
        "USER", NULL },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
};

static void
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

static void
message_cb (TpMessage *message)
{
    TpContact *sender = tp_signalled_message_get_sender (message);
    GDateTime *received = g_date_time_new_from_unix_utc (
        tp_message_get_received_timestamp (message));
    gchar *timestamp = g_date_time_format (received, "%Y-%m-%d %H:%M:%S");
    const gchar *sender_identifier = tp_contact_get_identifier (sender);
    const gchar *sender_alias = tp_contact_get_alias (sender);

    g_print ("Message-Token: %s\n", tp_message_get_token (message));
    g_print ("From: \"%s\" <%s>\n", sender_alias, sender_identifier);
    g_print ("Date: %s\n", timestamp);

    g_free (timestamp);

    unsigned int parts = tp_message_count_parts (message);
    for (unsigned int i = 1; i < parts; i++) {
        const GHashTable *part = tp_message_peek (message, i);
        const gchar *content_type = g_value_get_string (
            g_hash_table_lookup ((GHashTable*)part, "content-type"));
        const gchar *content = g_value_get_string (
            g_hash_table_lookup ((GHashTable*)part, "content"));

        g_print ("\n- #%d %s\n", i, content_type);
        g_print ("%s\n", content);
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

static void
channel_ready (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    (void) user_data;

    GError *error = NULL;
    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    TpChannel *channel = TP_CHANNEL (source);

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

            if (iter->next) {
                g_print ("\n\n");
            }
        }

        if (acknowledge) {
            tp_text_channel_ack_messages_async (TP_TEXT_CHANNEL (channel),
                                                messages,
                                                ack_messages_cb,
                                                NULL);
        }

        g_list_free_full (messages, g_object_unref);
    } else {
        g_printerr (
            "error: %s is not a text channel\n",
            tp_channel_get_identifier (channel));
    }

    pending -= 1;
    if (pending == 0)
    {
        g_main_loop_quit (loop);
    }
}

static void
channel_cb(TpChannel *channel)
{
    const char *type = tp_channel_get_channel_type (channel);
    const char *ident = tp_channel_get_identifier (channel);

    // if this is a text channel probe it for pending messages
    if (!strcmp (type, TP_IFACE_CHANNEL_TYPE_TEXT))
    {
        if (list_messages) {
            g_print ("%s\n", ident);
            return;
        }

        pending += 1;

        GQuark features[] = { TP_TEXT_CHANNEL_FEATURE_INCOMING_MESSAGES, 0};

        tp_proxy_prepare_async (
            channel,
            features,
            channel_ready,
            NULL);
    } else {
        if (verbose > 0) {
            g_printerr ("ignored channel %s %s\n", ident, type);
        }
    }
}

static void
connection_cb (TpConnection *connection,
               guint         status)
{
    TpContact *self;
    if (status == 0) {
        if (verbose > 0) {
            self = tp_connection_get_self_contact (connection);

            g_printerr (
                "connection ready: %s/%s (%s)\n",
                tp_connection_get_cm_name (connection),
                tp_connection_get_protocol_name(connection),
                tp_contact_get_identifier (self));
        }
    }
}


int
main (int argc, char **argv)
{
    TpSimpleClientFactory *factory = NULL;
    GQuark contact_features[] = { TP_CONTACT_FEATURE_ALIAS,
                                  TP_CONTACT_FEATURE_PRESENCE,
                                  0 };

    /* Parse commandline arguments */
    GOptionContext* context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free (context);

    loop = g_main_loop_new (NULL, FALSE);

    factory = (TpSimpleClientFactory*) tp_automatic_client_factory_new (NULL);

    tp_simple_client_factory_add_contact_features (factory,
                                                   3,
                                                   contact_features);
    for_each_channel_cb = channel_cb;
    for_each_connection_cb = connection_cb;
    tpic_run (factory);

    g_main_loop_run (loop);

    g_object_unref (factory);

    return 0;
}
