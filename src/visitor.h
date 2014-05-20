#ifndef CH_VISITOR_H
#define CH_VISITOR_H

struct _ChVisitor;
typedef struct _ChVisitor ChVisitor;

typedef void (*VisitChannelCb) (ChVisitor *visitor, TpChannel *channel);
typedef void (*VisitContactCb) (ChVisitor *visitor, TpContact *contact);
typedef void (*VisitConnectionCb) (ChVisitor *visitor, TpConnection *connection, guint status);
typedef void (*DisposeCb) (ChVisitor *visitor);

struct _ChVisitor {
    TpSimpleClientFactory *client;
    unsigned int pending;

    VisitChannelCb         visit_channel;
    VisitContactCb         visit_contact;
    VisitConnectionCb      visit_connection;
    DisposeCb              dispose;
};

ChVisitor*
ch_visitor_new (TpSimpleClientFactory *client);

void
ch_visitor_visit_connections (ChVisitor *self);

void
ch_visitor_visit_channels (ChVisitor *self, TpConnection *connection);

void
ch_visitor_visit_connection_contact (ChVisitor    *self,
                                     TpConnection *connection,
                                     char         *contact_id);

void
ch_visitor_visit_channel_contacts (ChVisitor *self, TpChannel *channel);

void
ch_visitor_visit_channel_target (ChVisitor *self, TpChannel *channel);

void
ch_visitor_incref (ChVisitor *self);

void
ch_visitor_decref (ChVisitor *self);

#endif
