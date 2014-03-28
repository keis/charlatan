#ifndef CH_VISITOR_H
#define CH_VISITOR_H

struct _ChVisitor;
typedef struct _ChVisitor ChVisitor;

typedef void (*VisitChannelCb) (ChVisitor *visitor, TpChannel *channel);
typedef void (*VisitConnectionCb) (ChVisitor *visitor, TpConnection *connection, guint status);
typedef void (*DisposeCb) (ChVisitor *visitor);

struct _ChVisitor {
    TpSimpleClientFactory *client;
    unsigned int pending;

    VisitChannelCb         visit_channel;
    VisitConnectionCb      visit_connection;
    DisposeCb              dispose;
};

void
ch_visitor_exec (ChVisitor *self, TpSimpleClientFactory *client);

void
ch_visitor_incref (ChVisitor *self);

void
ch_visitor_decref (ChVisitor *self);

#endif
