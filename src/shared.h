#ifndef CH_SHARED_H
#define CH_SHARED_H

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
