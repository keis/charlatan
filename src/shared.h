#ifndef CH_SHARED_H
#define CH_SHARED_H

void
list_connections_async (TpAccountManager      *client,
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


void
list_accounts_async (TpAccountManager    *manager,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data);

gboolean
list_accounts_finish (GObject       *source,
                      GAsyncResult  *result,
                      GList        **accounts,
                      GError       **error);
#endif
