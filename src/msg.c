#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>
#include <time.h>

#include "shared.h"

static char verbose;

/* commandline arguments */
const
GOptionEntry entries[] = {
    { "verbose",  'v', 0, G_OPTION_ARG_NONE,   &verbose,
        "Whether to print all messages or just errors.", NULL },
    { NULL,      0, 0, 0, NULL, NULL, NULL }
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
message_cb(GPtrArray *message)
{
	GHashTable *header = g_ptr_array_index (message, 0);

	const gchar *sender = g_value_get_string (
		g_hash_table_lookup (header, "message-sender-id"));
	const gchar *token = g_value_get_string (
		g_hash_table_lookup (header, "message-token"));
	gint64 received = g_value_get_int64 (
		g_hash_table_lookup (header, "message-received"));

	char receiveds[64];
	struct tm *timeinfo = localtime (&received);
	strftime (receiveds, sizeof(receiveds), "%Y-%m-%d %H:%M:%S", timeinfo);

	g_print ("Message-Token: %s\n", token);
	g_print ("From: %s\n", sender);
	g_print ("Date: %s\n", receiveds);

	for (int i = 1; i < message->len; i++) {
		GHashTable *part = g_ptr_array_index (message, i);
		const gchar *content_type = g_value_get_string (
			g_hash_table_lookup (part, "content-type"));
		g_print ("- #%d %s\n",
			i, content_type);
		g_print ("%s\n\n",
			g_value_get_string (g_hash_table_lookup (part, "content")));
	}
}

static void
get_pending_messages_cb (TpProxy *proxy,
	const GValue *value, const GError *in_error,
	gpointer user_data, GObject *weak_obj)
{
	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	GPtrArray *messages = g_value_get_boxed (value);
	for (int i = 0; i < messages->len; i++) {
		message_cb (g_ptr_array_index (messages, i));
	}

	pending -= 1;
	if (pending == 0)
	{
		g_main_loop_quit (loop);
	}
}

static void
channel_ready (TpChannel	*channel,
	       const GError	*in_error,
	       gpointer		 user_data)
{
	TpConnection *connection = (TpConnection*) user_data;

	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	if (verbose > 0) {
		g_printerr (" > channel_ready (%s)\n",
			tp_channel_get_identifier (channel));
	}

	// request pending messages
	pending += 1;
	tp_cli_dbus_properties_call_get (channel, -1,
			TP_IFACE_CHANNEL_INTERFACE_MESSAGES,
			"PendingMessages",
			get_pending_messages_cb,
			NULL, NULL, NULL);

	pending -= 1;
	if (pending == 0)
	{
		g_main_loop_quit (loop);
	}
}

static void
channel_cb(TpConnection *connection,
	const gchar *object_path,
	GHashTable *map)
{
	const char *type = tp_asv_get_string (map, TP_PROP_CHANNEL_CHANNEL_TYPE);
	const char *targetid = tp_asv_get_string (map, TP_PROP_CHANNEL_TARGET_ID);

	// if this is a text channel probe it for pending messages
	if (!strcmp (type, TP_IFACE_CHANNEL_TYPE_TEXT))
	{
		GError *error = NULL;
		pending += 1;

		TpChannel *channel = tp_channel_new_from_properties (
			connection, object_path, map,
			&error);
		if (error) {
			g_printerr ("error: %s\n", error->message);
			g_error_free (error);
		}

		tp_channel_call_when_ready (channel,
			channel_ready, (gpointer) connection);
	} else {
		if (verbose > 0) {
			g_printerr ("ignored channel %s %s\n", targetid, type);
		}
	}
}

static void
connection_cb(TpConnection *connection,
	guint status)
{
	if (status == 0) {
		if (verbose > 0) {
			g_printerr ("connection ready: %s/%s\n",
				tp_connection_get_connection_manager_name (connection),
				tp_connection_get_protocol_name(connection));
		}
	}
}


int
main (int argc, char **argv)
{
	TpDBusDaemon *busd = NULL;
	GError *error = NULL;

	g_type_init ();

    /* Parse commandline arguments */
    GOptionContext* context = g_option_context_new (NULL);
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_parse(context, &argc, &argv, NULL);
    g_option_context_free(context);

	loop = g_main_loop_new (NULL, FALSE);

	busd = tp_dbus_daemon_dup (&error);
	if (!busd)
		g_error ("%s", error->message);

	for_each_channel_cb = channel_cb;
	for_each_connection_cb = connection_cb;
	tpic_run (busd);

	g_main_loop_run (loop);

	g_object_unref (busd);

	return 0;
}
