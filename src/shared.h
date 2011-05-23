#ifndef TPT_SHARED_H
#define TPT_SHARED_H

typedef void (*ForEachChannelCb) (TpConnection *connection,
	const char *object_path,
	GHashTable *channel_info);

/* globals variables.. so shoot me (should perhaps be but in a struct/GObject */
extern ForEachChannelCb for_each_channel_cb;
extern GMainLoop *loop;
extern unsigned int pending;

void
for_each_channel(TpDBusDaemon *bus);

#endif
