#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>

#include "visitor.h"

GMainLoop *loop;

static char verbose;

/* command line arguments */
const GOptionEntry entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
        "Whether to print all messages or just errors.", NULL },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
};

static void
contacts_ready (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    ChVisitor *visitor = (ChVisitor*) user_data;
    GError *error = NULL;
    GPtrArray *contacts;

    if (!tp_simple_client_factory_upgrade_contacts_finish (
            TP_SIMPLE_CLIENT_FACTORY (source), result, &contacts, &error)
    ) {
        g_printerr ("error: %s\n", error->message);
    }

    for (unsigned int i = 0; i < contacts->len; i++)
    {
        TpContact *contact = g_ptr_array_index (contacts, i);

        if (tp_contact_get_presence_type (contact) != TP_CONNECTION_PRESENCE_TYPE_OFFLINE) {
            g_print (
                "%s\t%s\n",
                tp_contact_get_identifier (contact),
                tp_contact_get_alias (contact));
        }
    }

    g_ptr_array_free (contacts, TRUE);
    ch_visitor_decref (visitor);
}

static void
channel_ready (GObject      *source,
               GAsyncResult *result,
               gpointer      user_data)
{
    ChVisitor *visitor = (ChVisitor*) user_data;

    GError *error = NULL;
    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    TpChannel *channel = TP_CHANNEL (source);
    TpConnection *connection = tp_channel_get_connection (channel);

    if (verbose > 0) {
        g_printerr (" > channel_ready (%s)\n",
            tp_channel_get_identifier (channel));
    }

    GPtrArray *contacts = tp_channel_group_dup_members_contacts (channel);

    if (contacts->len > 0) {
        ch_visitor_incref (visitor);
        tp_simple_client_factory_upgrade_contacts_async (
            tp_proxy_get_factory (connection),
            connection,
            contacts->len,
            (TpContact * const *) contacts->pdata,
            contacts_ready,
            visitor);
    }

    g_ptr_array_free (contacts, TRUE);
    ch_visitor_decref (visitor);
}

static void
channel_cb (ChVisitor *visitor,
            TpChannel *channel)
{
    /* if this channel is a contact list, we want to know
     * about it */
    const char *type = tp_channel_get_channel_type (channel);
    const char *ident = tp_channel_get_identifier (channel);

    if (!strcmp (ident, "subscribe") &&
        !strcmp (type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST))
    {
        ch_visitor_incref (visitor);
        tp_proxy_prepare_async (
            channel,
            NULL,
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

    TpContact *self;
    if (status == 0) {
        if (verbose > 0) {
            self = tp_connection_get_self_contact (connection);

            g_printerr (
                "connection ready: %s/%s (%s)\n",
                tp_connection_get_cm_name (connection),
                tp_connection_get_protocol_name(connection),
                self ? tp_contact_get_identifier (self) : "n/a");
        }
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
    ChVisitor visitor;

    TpSimpleClientFactory *factory = NULL;
    GQuark contact_features[] = { TP_CONTACT_FEATURE_ALIAS,
                                  TP_CONTACT_FEATURE_PRESENCE,
                                  0 };

    /* Parse command line arguments */
    GOptionContext* context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free (context);

    loop = g_main_loop_new (NULL, FALSE);

    factory = (TpSimpleClientFactory*) tp_automatic_client_factory_new (NULL);

    tp_simple_client_factory_add_contact_features (factory,
                                                   3,
                                                   contact_features);

    visitor.visit_channel = channel_cb;
    visitor.visit_connection = connection_cb;
    visitor.dispose = dispose_cb;
    ch_visitor_exec (&visitor, factory);

    g_main_loop_run (loop);

    g_object_unref (factory);

    return 0;
}
