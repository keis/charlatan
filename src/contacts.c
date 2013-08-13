#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>
#include <string.h>

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

	if (verbose > 0) {
		g_printerr (" > channel_ready (%s)\n",
			tp_channel_get_identifier (channel));
	}

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
channel_cb(TpConnection *connection,
	const gchar *object_path,
	GHashTable *map)
{
	const char *type = tp_asv_get_string (map, TP_PROP_CHANNEL_CHANNEL_TYPE);
	const char *targetid = tp_asv_get_string (map, TP_PROP_CHANNEL_TARGET_ID);

	/* if this channel is a contact list, we want to know
	 * about it */
	if (!strcmp (targetid, "subscribe") &&
		!strcmp (type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST))
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
				tp_connection_get_cm_name (connection),
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
