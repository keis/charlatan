#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <telepathy-glib/telepathy-glib.h>

#include "visitor.h"

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

    GError *error = NULL;
    TpConnection *connection = TP_CONNECTION (source);

    if (!tp_proxy_prepare_finish (source, result, &error)) {
        g_printerr ("error: %s\n", error->message);
    }

    // request the current channels if needed
    if (self->visit_channel) {
        ch_visitor_incref (self);
        list_channels_async (connection, channel_list_cb, user_data);
    }

    // Run callback, 0 indicates failure reason
    if (self->visit_connection) {
        self->visit_connection (self, connection, 0);
    }

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
}

void
ch_visitor_exec (ChVisitor *self,
                 TpSimpleClientFactory *client)
{
    self->client = client;
    self->pending = 0;
    list_connections_async (client, connection_list_cb, self);
}
