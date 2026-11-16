/* Copyright (c) 2026 bhgv
 * EN: Bounded owner-thread deferred byte reads, included by original dispatcher.
 * RU: Bounded отложенные byte reads на owner-thread, включаются исходным dispatcher.
 */

/* EN/RU: One request retains scalars and borrowed fid until owner-thread retirement.
 * Один request хранит scalars и borrowed fid до owner-thread retirement. */
struct StyxRead
{
    StyxRead *next; /* Dispatcher-owned pending chain. */
    StyxRead *adopted; /* Stack stage points at accepted heap request. */
    Client *client; /* Borrowed live session; nil before release callback. */
    Fid *fid; /* Borrowed held object; retirement precedes its freefid. */
    void *aux; /* Adopted release context only on successful defer. */
    void (*release)(void*); /* Nonwaiting context cleanup exactly once. */
    ulong limit; /* Negotiated/request capacity captured before callback. */
    vlong offset; /* Exact original64-bit offset. */
    ushort tag; /* Original leased request tag. */
    int references; /* Dispatcher and caller refs, serialized on native owner. */
    int waiting; /* Callback returned; completion may publish now. */
    int directory; /* Directory callbacks cannot use deferred byte reads. */
};


/* EN/RU: See styxserver.h; request can safely outlive fid/client after detachment.
 * См. styxserver.h; request может пережить fid/client после detach. */
void
styxreadfree(StyxRead *request)
{
    if(request != nil && --request->references == 0)
        free(request);
}


/* EN/RU: Detach before native release; cleanup cannot observe an active request.
 * Отсоединить до native release; cleanup не видит активный request. */
static void
retireread(StyxRead *request)
{
    StyxRead **link;
    Client *client = request->client;
    for(link = &client->reads; *link != nil; link = &(*link)->next){
        if(*link == request){
            *link = request->next;
            break;
        }
    }
    request->next = nil;
    request->client = nil;
    request->fid = nil;
    if(request->release != nil)
        request->release(request->aux);
    styxreadfree(request);
}


/* EN/RU: See styxserver.h; allocate only on actual defer, never on synchronous reads.
 * См. styxserver.h; allocation только при defer, не для synchronous reads. */
StyxRead*
styxdeferread(Styxserver *server, void *aux, void (*release)(void*))
{
    StyxRead *stage, *node, *request;
    Client *client;
    int count = 0;
    if(server == nil || (client = server->curc) == nil || (stage = client->reading) == nil ||
       stage->adopted != nil || stage->directory || !stage->limit || stage->tag == NOTAG)
        return nil;
    for(node = client->reads; node != nil; node = node->next){
        if(node->fid == stage->fid)
            return nil;
        count++;
    }
    if(count >= 128 || (request = calloc(1, sizeof(*request))) == nil)
        return nil;
    *request = *stage;
    request->references = 2;
    request->aux = aux;
    request->release = release;
    request->next = client->reads;
    client->reads = request;
    stage->adopted = request;
    return request;
}


/* EN/RU: See styxserver.h; staged callback cannot complete/reenter the dispatcher.
 * См. styxserver.h; staged callback не завершает/не вызывает dispatcher повторно. */
Client*
styxreadclient(StyxRead *request)
{
    return request != nil && request->waiting ? request->client : nil;
}


/* EN/RU: See styxserver.h; fid aux is borrowed only while request remains pending.
 * См. styxserver.h; fid aux заимствован только пока request pending. */
void*
styxreadaux(StyxRead *request)
{
    return styxreadclient(request) != nil ? request->fid->aux : nil;
}


/* EN/RU: See styxserver.h; live limit is nonzero and fits original negotiated msize.
 * См. styxserver.h; live limit ненулевой и ограничен negotiated msize. */
ulong
styxreadlimit(StyxRead *request)
{
    return styxreadclient(request) != nil ? request->limit : 0;
}


/* EN/RU: See styxserver.h; preserve the complete original offset without narrowing.
 * См. styxserver.h; сохранить original offset полностью без narrowing. */
vlong
styxreadoffset(StyxRead *request)
{
    return styxreadclient(request) != nil ? request->offset : 0;
}


/* EN/RU: See styxserver.h; invalid completion is nonconsuming, valid response retires once.
 * См. styxserver.h; invalid completion не поглощает request, valid завершает один раз. */
int
styxreadreply(StyxRead *request, void *bytes, ulong count, char *error)
{
    Client *client = styxreadclient(request);
    Fcall reply;
    if(client == nil || count > request->limit || (count && (bytes == nil || error != nil)))
        return 0;
    memset(&reply, 0, sizeof(reply));
    reply.tag = request->tag;
    reply.type = error != nil ? Rerror : Rread;
    reply.ename = error;
    reply.count = count;
    reply.data = bytes;
    wr(client, &reply);
    retireread(request);
    return 1;
}


/* EN/RU: Pending tags cannot be reused by ordinary requests or their flush envelopes.
 * Pending tags не переиспользуются ordinary requests либо flush envelopes. */
static int
readtag(Client *client, ushort tag)
{
    StyxRead *request;
    for(request = client->reads; request != nil; request = request->next){
        if(request->tag == tag)
            return 1;
    }
    return 0;
}


/* EN/RU: Matching Tflush cancels silently before Rflush; unrelated work remains pending.
 * Совпавший Tflush отменяет до Rflush; остальные requests остаются pending. */
static void
flushread(Client *client, ushort tag)
{
    StyxRead *request;
    for(request = client->reads; request != nil; request = request->next){
        if(request->tag == tag){
            retireread(request);
            return;
        }
    }
}


/* EN/RU: Nil fid aborts a session silently; one fid retires reads with Rerror before freefid.
 * Nil fid отменяет session без ответа; один fid завершает reads с Rerror до freefid. */
static void
cancelreads(Client *client, Fid *fid)
{
    StyxRead *request, *next;
    for(request = client->reads; request != nil; request = next){
        next = request->next;
        if(fid == nil || request->fid == fid){
            if(fid != nil && !(client->state & CDISC))
                styxreadreply(request, nil, 0, "fid retired");
            else
                retireread(request);
        }
    }
}


/* EN/RU: Stage scalars on stack; existing read callback remains synchronous unless it opts in.
 * Scalars на stack; существующий read callback synchronous пока сам не выберет defer. */
static int
callbackread(Client *client, Fid *fid, Fcall *call, ulong *count, int directory)
{
    StyxRead stage, *request;
    memset(&stage, 0, sizeof(stage));
    stage.client = client;
    stage.fid = fid;
    stage.limit = call->count;
    stage.offset = call->offset;
    stage.tag = call->tag;
    stage.directory = directory;
    client->reading = &stage;
    call->ename = client->server->ops->read(fid->qid, client->data, count, call->offset);
    client->reading = nil;
    request = stage.adopted;
    if(request == nil)
        return 0;
    request->waiting = 1;
    if(call->ename == nil && *count == 0)
        return 1;
    if(call->ename == nil)
        call->ename = "deferred read returned immediate data";
    retireread(request);
    return 0;
}
