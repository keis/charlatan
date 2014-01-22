#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>

#include "shared.h"

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
    (void) user_data;
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

    pending -= 1;
    if (pending == 0)
    {
        g_main_loop_quit (loop);
    }
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
    TpConnection *connection = tp_channel_get_connection (channel);

    if (verbose > 0) {
        g_printerr (" > channel_ready (%s)\n",
            tp_channel_get_identifier (channel));
    }

    GPtrArray *contacts = tp_channel_group_dup_members_contacts (channel);

    if (contacts->len > 0) {
        pending += 1;
        tp_simple_client_factory_upgrade_contacts_async (
            tp_proxy_get_factory (connection),
            connection,
            contacts->len,
            (TpContact * const *) contacts->pdata,
            contacts_ready,
            NULL);
    }

    g_ptr_array_free (contacts, TRUE);

    pending -= 1;
    if (pending == 0)
    {
        g_main_loop_quit (loop);
    }
}

static void
channel_cb(TpChannel *channel)
{
    /* if this channel is a contact list, we want to know
     * about it */
    const char *type = tp_channel_get_channel_type (channel);
    const char *ident = tp_channel_get_identifier (channel);

    if (!strcmp (ident, "subscribe") &&
        !strcmp (type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST))
    {
        pending += 1;
        tp_proxy_prepare_async (
            channel,
            NULL,
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
    if (status == 0) {
        if (verbose > 0) {
            g_printerr (
                "connection ready: %s/%s\n",
                tp_connection_get_cm_name (connection),
                tp_connection_get_protocol_name(connection));
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

    for_each_channel_cb = channel_cb;
    for_each_connection_cb = connection_cb;
    tpic_run (factory);

    g_main_loop_run (loop);

    g_object_unref (factory);

    return 0;
}
