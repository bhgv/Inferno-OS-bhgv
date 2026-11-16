/* Copyright (c) 2026 bhgv
 * EN: Owner-thread deferred attach transactions; included by original dispatcher.
 * RU: Отложенные attach-транзакции socket-owner; включаются исходным dispatcher.
 */

/* EN/RU: Server and caller references are serialized on one native socket owner.
 * Ссылки server/caller сериализованы одним native socket-owner. */
struct StyxAttach
{
    Client *client; /* Borrowed until retirement, cleared before release callback. */
    char *uname; /* Owned requested effective name until commit/retirement. */
    char *aname; /* Owned logical selection until commit/retirement. */
    void *aux; /* Adopted native release context after successful defer. */
    void (*release)(void*); /* Consumes aux exactly once on transaction retirement. */
    u32int fid; /* Reserved attach fid; no Fid allocated before successful reply. */
    Qid root; /* Explicit native selected attach root, default directory Qroot. */
    ushort tag; /* Original wire transaction tag. */
    int references; /* One dispatcher ref plus at most one caller ref. */
    int deferred; /* Caller adopted its request handle. */
    int waiting; /* Callback returned; completion may now publish a reply. */
};


/* EN/RU: Consume one serialized reference; inactive request can outlive its client.
 * Поглотить ссылку; inactive request может пережить client, NULL допустим. */
void
styxattachfree(StyxAttach *request)
{
    if(request != nil && --request->references == 0){
        free(request->uname);
        free(request->aname);
        free(request);
    }
}


/* EN/RU: Detach before native cleanup; no callback can subsequently complete it.
 * Отсоединить до native cleanup; после этого завершение больше невозможно. */
static void
cancelattach(Client *client)
{
    StyxAttach *request = client->attach;
    if(request == nil)
        return;
    client->attach = nil;
    request->client = nil;
    if(request->release != nil)
        request->release(request->aux);
    styxattachfree(request);
}


/* EN/RU: Adopt aux only during attach callback; return owned handle/nil unchanged.
 * Принять aux только в attach callback; свой handle/nil, при отказе aux не принят. */
StyxAttach*
styxdeferattach(Styxserver *server, void *aux, void (*release)(void*))
{
    StyxAttach *request;
    if(server == nil || server->curc == nil)
        return nil;
    request = server->curc->attach;
    if(request == nil || request->waiting || request->deferred)
        return nil;
    request->deferred = 1;
    request->references++;
    request->aux = aux;
    request->release = release;
    return request;
}


/* EN/RU: Borrow live pending client only on its owner thread, nil otherwise.
 * Заимствовать live pending client только на owner-thread, иначе nil. */
Client*
styxattachclient(StyxAttach *request)
{
    return request != nil && request->waiting ? request->client : nil;
}


/* EN/RU: Select root Qid on socket owner; NULL pending means current attach callback.
 * Reject inactive/foreign/auth roots and cached-node type conflicts; no wire reply or authority grant.
 * Выбрать root Qid на socket owner; NULL pending — текущий attach callback;0/-1 success/failure. */
int
styxattachroot(Styxserver *server, StyxAttach *pending, Qid qid)
{
    Styxfile *cached;
    if(server == nil)
        return -1;
    if(pending == nil){
        pending = server->curc != nil ? server->curc->attach : nil;
        if(pending != nil && pending->waiting)
            return -1;
    }
    if(pending == nil || pending->client == nil || pending->client->server != server ||
       (qid.type & QTAUTH))
        return -1;
    cached = styxfindfile(server, qid.path);
    if(cached != nil && ((cached->d.qid.type ^ qid.type) & QTDIR))
        return -1;
    pending->root = qid;
    return 0;
}


/* EN/RU: Complete once with original tag; nil error commits identity and fid.
 * Завершить один раз с исходным tag; nil error фиксирует identity и fid. */
int
styxattachreply(StyxAttach *request, char *error)
{
    Client *client = styxattachclient(request);
    Fcall reply;
    if(client == nil)
        return 0;
    memset(&reply, 0, sizeof(reply));
    reply.tag = request->tag;
    reply.type = Rerror;
    if(error == nil && findfid(client, request->fid) != nil)
        error = "fid in use";
    if(error == nil){
        free(client->uname);
        free(client->aname);
        client->uname = request->uname;
        client->aname = request->aname;
        request->uname = request->aname = nil;
        reply.qid = request->root;
        newfid(client, request->fid, reply.qid);
        reply.type = Rattach;
    }
    reply.ename = error;
    wr(client, &reply);
    cancelattach(client);
    return 1;
}


/* EN/RU: Stage copied identity, call native admission and defer or complete.
 * Подготовить copied identity, вызвать native admission, отложить либо завершить. */
static void
beginattach(Client *client, Fcall *call)
{
    StyxAttach *request;
    char *uname, *aname, *error;
    call->type = Rerror;
    if(client->attach != nil)
        call->ename = "attach already pending";
    else if(findfid(client, call->fid) != nil)
        call->ename = "fid in use";
    else if(call->afid != NOFID)
        call->ename = "authentication fid not supported";
    else if(client->fids != nil &&
        ((call->uname[0] && strcmp(call->uname, client->uname)) ||
         (call->aname[0] && strcmp(call->aname, client->aname))))
        call->ename = "connection identity already in use";
    else{
        request = calloc(1, sizeof(*request));
        if(request == nil){
            call->ename = Enomem;
            wr(client, call);
            return;
        }
        request->root = styxqid(Qroot, 1);
        request->references = 1;
        request->client = client;
        request->fid = call->fid;
        request->tag = call->tag;
        request->uname = strdup(call->uname[0] ? call->uname : client->uname);
        request->aname = strdup(call->aname[0] ? call->aname : client->aname);
        client->attach = request;
        uname = client->uname;
        aname = client->aname;
        client->uname = request->uname;
        client->aname = request->aname;
        error = Enomem;
        if(client->uname != nil && client->aname != nil)
            error = client->server->ops->attach != nil ?
                client->server->ops->attach(client->uname, client->aname) : nil;
        client->uname = uname;
        client->aname = aname;
        request->waiting = 1;
        if(error != nil || !request->deferred)
            styxattachreply(request, error);
        return;
    }
    wr(client, call);
}
