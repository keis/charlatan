#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>

#include "shared.h"

ForEachChannelCb for_each_channel_cb;
GMainLoop *loop = NULL;
unsigned int pending = 0;

static void
get_channels_cb (TpProxy *proxy,
	const GValue *value, const GError *in_error,
	gpointer user_data, GObject *weak_obj)
{
	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	TpConnection *connection = (TpConnection*) user_data;

	g_return_if_fail (G_VALUE_HOLDS (value, TP_ARRAY_TYPE_CHANNEL_DETAILS_LIST));

	GPtrArray *channels = g_value_get_boxed (value);
	for (int i = 0; i < channels->len; i++) {
		GValueArray *channel = g_ptr_array_index (channels, i);
		char *object_path;
		GHashTable *map;

		tp_value_array_unpack (channel, 2, &object_path, &map);

		for_each_channel_cb (connection, object_path, map);
	}

	pending -= 1;
	if (pending == 0)
	{
		g_main_loop_quit (loop);
	}
}

static void
connection_ready (TpConnection *connection,
	const GError *in_error, gpointer user_data)
{
	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	g_printerr ("connection ready\n");

	/* request the current channels */
	tp_cli_dbus_properties_call_get (connection, -1,
			TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
			"Channels",
			get_channels_cb,
			(gpointer)connection, NULL, NULL);
}

static void
connection_status (TpConnection *connection,
	guint status, const GError *in_error,
	gpointer user_data, GObject *weak_object)
{

	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	if (status != 0) {
		pending -= 1;
	}
}

static void
connection_names (const gchar * const *names, gsize n_names,
	const gchar * const *cms, const gchar * const *protols,
	const GError *in_error, gpointer user_data, GObject *weak_object)
{
	TpDBusDaemon *bus = (TpDBusDaemon*)user_data;

	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	TpConnection *connection;
	GError *error = NULL;

	for(int i = 0; i < n_names; i++) {
		const gchar *name = names[i];
		pending += 1;
		connection = tp_connection_new (bus, name, NULL, &error);

		tp_connection_call_when_ready (connection, connection_ready, NULL);
		tp_cli_connection_call_get_status (connection, -1,
			connection_status, NULL, NULL, NULL);
	}
}


void
for_each_channel(TpDBusDaemon *bus)
{
	tp_list_connection_names (bus, connection_names,
		bus, NULL, NULL);
}
