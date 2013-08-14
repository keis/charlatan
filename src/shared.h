#ifndef TPT_SHARED_H
#define TPT_SHARED_H

typedef void (*ForEachChannelCb) (TpConnection *connection,
                                  const char *object_path,
                                  GHashTable *channel_info);

typedef void (*ForEachConnectionCb) (TpConnection *connection, guint status);

/* globals variables.. so shoot me (should perhaps be but in a struct/GObject */
extern ForEachChannelCb for_each_channel_cb;
extern ForEachConnectionCb for_each_connection_cb;
extern GMainLoop *loop;
extern unsigned int pending;

void
list_connections_async (TpSimpleClientFactory *client,
                        GAsyncReadyCallback    callback,
                        gpointer               user_data);
gboolean
list_connections_finish (GObject       *source,
                         GAsyncResult  *result,
                         GPtrArray    **connections,
                         GError       **error);


void
tpic_run (TpSimpleClientFactory *client);

#endif
