#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>

GMainLoop *loop = NULL;
TpDBusDaemon *busd = NULL;

unsigned int pending = 0;

static void
contacts_ready (TpConnection *conn,
	guint n_contacts, TpContact * const	*contacts,
	guint n_failed, const TpHandle *failed,
	const GError *in_error, gpointer user_data,
	GObject	*weak_obj)
{
	TpChannel *channel = TP_CHANNEL (user_data);

	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	g_assert (pending >= n_contacts);

	int i;
	for (i = 0; i < n_contacts; i++)
	{
		TpContact *contact = contacts[i];

		if (tp_contact_get_presence_type (contact) != TP_CONNECTION_PRESENCE_TYPE_OFFLINE) {
			g_print ("%s %s\n",
				tp_contact_get_identifier (contact),
				tp_contact_get_alias (contact));
		}
	}
	pending -= n_contacts;
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

	g_printerr (" > channel_ready (%s)\n",
			tp_channel_get_identifier (channel));

	const TpIntSet *members = tp_channel_group_get_members (channel);
	GArray *handles = tp_intset_to_array (members);

	if (handles->len > 0) {
		pending += handles->len;
		/* we want to create a TpContact for each member of this channel */
		static const TpContactFeature features[] = {
			TP_CONTACT_FEATURE_ALIAS,
			TP_CONTACT_FEATURE_PRESENCE
		};

		tp_connection_get_contacts_by_handle (connection,
				handles->len, (const TpHandle *) handles->data,
				G_N_ELEMENTS (features), features,
				contacts_ready,
				channel, NULL, NULL);
	}

	g_array_free (handles, TRUE);

	pending -= 1;
	if (pending == 0)
	{
		g_main_loop_quit (loop);
	}
}

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
		GError *error = NULL;
		GValueArray *channel = g_ptr_array_index (channels, i);
		char *object_path;
		GHashTable *map;

		tp_value_array_unpack (channel, 2, &object_path, &map);

		const char *type = tp_asv_get_string (map, TP_PROP_CHANNEL_CHANNEL_TYPE);
		const char *targetid = tp_asv_get_string (map, TP_PROP_CHANNEL_TARGET_ID);

		/* if this channel is a contact list, we want to know
		 * about it */
		if (!strcmp (targetid, "subscribe") &&
			!strcmp (type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST))
		{
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
			g_printerr ("ignored channel %s %s\n", targetid, type);
		}
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

void
connection_names (const  gchar * const *names, gsize n_names,
	const gchar * const *cms, const gchar * const *protols,
	const GError *in_error, gpointer user_data, GObject *weak_object)
{

	if (in_error) {
		g_printerr ("error: %s\n", in_error->message);
		return;
	}

	TpConnection *connection;
	GError *error = NULL;

	for(int i = 0; i < n_names; i++) {
		const gchar *name = names[i];
		pending += 1;
		connection = tp_connection_new (busd, name, NULL, &error);

		tp_connection_call_when_ready (connection, connection_ready, NULL);
		tp_cli_connection_call_get_status (connection, -1,
			connection_status, NULL, NULL, NULL);
	}
}

int
main (int argc, char **argv)
{
	GError *error = NULL;

	g_type_init ();

	loop = g_main_loop_new (NULL, FALSE);

	busd = tp_dbus_daemon_dup (&error);
	if (!busd)
		g_error ("%s", error->message);
	
	tp_list_connection_names (busd, connection_names,
		NULL, NULL, NULL);

	g_main_loop_run (loop);

	g_object_unref (busd);

	return 0;
}
