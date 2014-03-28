#ifndef CH_SHARED_H
#define CH_SHARED_H

/* globals variables.. so shoot me (should perhaps be but in a struct/GObject */
extern GMainLoop *loop;

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
list_channels_async (TpConnection        *connection,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data);
gboolean
list_channels_finish (GObject       *source,
                      GAsyncResult  *result,
                      GPtrArray    **channels,
                      GError       **error);
#endif
