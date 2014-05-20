#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <telepathy-glib/telepathy-glib.h>

#include "visitor.h"
#include "shared.h"

void
ch_visitor_incref (ChVisitor *self)
{
    self->pending += 1;
}

void
ch_visitor_decref (ChVisitor *self)
{
    self->pending -= 1;
    if (self->pending == 0) {
        self->dispose (self);
    }
}

static void
channel_list_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    ChVisitor *self = (ChVisitor*) user_data;

    GError *error = NULL;
    GPtrArray *channels;

    if (!list_channels_finish (source, result, &channels, &error)) {
        g_printerr ("error: %s\n", error->message);
        return;
    }

    for (unsigned int i = 0; i < channels->len; i++)
    {
        TpChannel *channel = g_ptr_array_index (channels, i);
        if (self->visit_channel) {
            self->visit_channel (self, channel);
        }
    }

    g_ptr_array_free (channels, TRUE);

    ch_visitor_decref (self);
}

static void
connection_ready (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    ChVisitor *self = (ChVisitor*) user_data;
    TpConnection *connection = TP_CONNECTION (source);
    GError *error = NULL;

    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
    }

    // Run callback, 0 indicates failure reason
    if (self->visit_connection) {
        self->visit_connection (self, connection, 0);
    }

    ch_visitor_decref (self);
}

static void
contacts_ready (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    ChVisitor *self = (ChVisitor*) user_data;
    TpSimpleClientFactory *factory = TP_SIMPLE_CLIENT_FACTORY (source);
    GError *error = NULL;
    GPtrArray *contacts;

    if (!tp_simple_client_factory_upgrade_contacts_finish (factory, result,
                                                           &contacts, &error)
    ) {
        g_printerr ("error: %s\n", error->message);
    }

    for (unsigned int i = 0; i < contacts->len; i++) {
        TpContact *contact = g_ptr_array_index (contacts, i);

        if (self->visit_contact) {
            self->visit_contact (self, contact);
        }
    }

    g_ptr_array_free (contacts, TRUE);
    ch_visitor_decref (self);
}

static void
connection_status (TpConnection *connection,
                   guint         status,
                   const GError *in_error,
                   gpointer      user_data,
                   GObject      *weak_object)
{
    (void) weak_object;
    ChVisitor *self = (ChVisitor*) user_data;

    if (in_error) {
        g_printerr ("error: %s\n", in_error->message);
    }

    // Run callback for inactive connections
    if (status != 0) {
        if (self->visit_connection) {
            self->visit_connection (self, connection, status);
        }

        ch_visitor_decref (self);
    }
}

static void
connection_list_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    ChVisitor *self = (ChVisitor*) user_data;

    GError *error = NULL;
    GPtrArray *connections;

    if (!list_connections_finish (source, result, &connections, &error)) {
        g_printerr ("error: %s\n", error->message);
        ch_visitor_decref (self);
        return;
    }

    for (unsigned int i = 0; i < connections->len; i++)
    {
        TpConnection *connection = g_ptr_array_index (connections, i);

        ch_visitor_incref (self);
        tp_proxy_prepare_async (connection, NULL, connection_ready, user_data);
        tp_cli_connection_call_get_status (connection, -1,
                                           connection_status, user_data,
                                           NULL, NULL);
    }

    g_ptr_array_free (connections, TRUE);
    ch_visitor_decref (self);
}

void
ch_visitor_visit_channels (ChVisitor *self,
                           TpConnection *connection)
{
    ch_visitor_incref (self);
    list_channels_async (connection, channel_list_cb, self);
}

void
ch_visitor_visit_channel_contacts (ChVisitor *self,
                                   TpChannel *channel)
{
    TpConnection *connection = tp_channel_get_connection (channel);
    GPtrArray *contacts = tp_channel_group_dup_members_contacts (channel);

    if (contacts->len > 0) {
        ch_visitor_incref (self);
        tp_simple_client_factory_upgrade_contacts_async (
            tp_proxy_get_factory (connection),
            connection,
            contacts->len,
            (TpContact * const *) contacts->pdata,
            contacts_ready,
            self);
    }

    g_ptr_array_free (contacts, TRUE);
}

void
ch_visitor_visit_channel_target (ChVisitor *self,
                                 TpChannel *channel)
{
    TpConnection *connection = tp_channel_get_connection (channel);
    TpContact *contact = tp_channel_get_target_contact (channel);
    GPtrArray *contacts = g_ptr_array_new ();

    g_ptr_array_add (contacts, (gpointer) contact);
    ch_visitor_incref (self);
    tp_simple_client_factory_upgrade_contacts_async (
        tp_proxy_get_factory (connection),
        connection,
        contacts->len,
        (TpContact * const *) contacts->pdata,
        contacts_ready,
        self);

    g_ptr_array_free (contacts, TRUE);
}

void
ch_visitor_visit_connections (ChVisitor *self)
{
    TpAccountManager *manager;

    ch_visitor_incref (self);
    manager = tp_account_manager_new_with_factory (self->client);
    list_connections_async (manager, connection_list_cb, self);
}

ChVisitor*
ch_visitor_new (TpSimpleClientFactory *client)
{
    ChVisitor *self = g_new (ChVisitor, 1);

    self->client = client;
    self->pending = 0;

    return self;
}
