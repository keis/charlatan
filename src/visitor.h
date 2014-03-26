#ifndef CH_VISITOR_H
#define CH_VISITOR_H

struct _ChVisitor;
typedef struct _ChVisitor ChVisitor;

typedef void (*VisitChannelCb) (ChVisitor *visitor, TpChannel *channel);
typedef void (*VisitConnectionCb) (ChVisitor *visitor, TpConnection *connection, guint status);

struct _ChVisitor {
    TpSimpleClientFactory *client;
    VisitChannelCb        visit_channel;
    VisitConnectionCb     visit_connection;
};

void
ch_visitor_exec (ChVisitor *self, TpSimpleClientFactory *client);

#endif
