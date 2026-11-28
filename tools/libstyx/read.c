/* Copyright (c) 2026 bhgv
 * EN: Deferred byte reads and writes, owned by the original dispatcher.
 * RU: Отложенные чтения и записи байтов под управлением исходного диспетчера.
 */

#define STYX_PENDING_DATA_LIMIT 128 /* Maximum deferred data requests for one client. */

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
    int operation; /* Original Tread or Twrite. */
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
    {
        free(request);
    }
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


/* EN: Create one caller-owned slot for operation and take aux with release
 * on success. The server borrows the fid and owns the second reference.
 * RU: Создать ссылку вызывающего для operation и принять aux с release
 * при успехе. Сервер заимствует fid и владеет второй ссылкой. */
static StyxRead *deferdata(Styxserver *server, int operation, void *aux, void (*release)(void *))
{
    StyxRead *stage, *node, *request;
    Client *client;
    int count = 0;
    if(server == nil || (client = server->curc) == nil || (stage = client->reading) == nil ||
       stage->operation != operation || stage->adopted != nil || stage->directory ||
       !stage->limit || stage->tag == NOTAG)
    {
        return nil;
    }
    for(node = client->reads; node != nil; node = node->next)
    {
        if(node->fid == stage->fid)
        {
            return nil;
        }
        count++;
    }
    if(count >= STYX_PENDING_DATA_LIMIT || (request = calloc(1, sizeof(*request))) == nil)
    {
        return nil;
    }
    *request = *stage;
    request->references = 2;
    request->aux = aux;
    request->release = release;
    request->next = client->reads;
    client->reads = request;
    stage->adopted = request;
    return request;
}


/* EN/RU: See styxserver.h. / См. styxserver.h. */
StyxRead *styxdeferread(Styxserver *server, void *aux, void (*release)(void *))
{
    return deferdata(server, Tread, aux, release);
}


/* EN/RU: See styxserver.h. / См. styxserver.h. */
StyxWrite *styxdeferwrite(Styxserver *server, void *aux, void (*release)(void *))
{
    return deferdata(server, Twrite, aux, release);
}


/* EN/RU: See styxserver.h; staged callback cannot complete/reenter the dispatcher.
 * См. styxserver.h; staged callback не завершает/не вызывает dispatcher повторно. */
Client*
styxreadclient(StyxRead *request)
{
    return request != nil && request->operation == Tread && request->waiting ? request->client : nil;
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


/* EN: Finish a live request using a confirmed count and optional read bytes
 * or an error. Borrow payload and error for encoding. Invalid input keeps it live.
 * RU: Завершить живой запрос с подтверждённым count, байтами чтения или ошибкой.
 * Заимствовать данные и ошибку до кодирования. При неверном входе сохранить запрос. */
static int replydata(StyxRead *request, void *bytes, ulong count, char *error)
{
    Fcall reply;
    if(request == nil || !request->waiting || request->client == nil || count > request->limit ||
       (count && (error != nil || (request->operation == Tread && bytes == nil))))
    {
        return 0;
    }
    memset(&reply, 0, sizeof(reply));
    reply.tag = request->tag;
    reply.type = error != nil ? Rerror : request->operation + 1;
    reply.ename = error;
    reply.count = count;
    reply.data = bytes;
    wr(request->client, &reply);
    retireread(request);
    return 1;
}


/* EN/RU: See styxserver.h. / См. styxserver.h. */
int styxreadreply(StyxRead *request, void *bytes, ulong count, char *error)
{
    return request != nil && request->operation == Tread && replydata(request, bytes, count, error);
}


/* EN/RU: See styxserver.h. / См. styxserver.h. */
int styxwritereply(StyxWrite *request, ulong count, char *error)
{
    return request != nil && request->operation == Twrite && replydata(request, nil, count, error);
}


/* EN/RU: See styxserver.h. / См. styxserver.h. */
void styxwritefree(StyxWrite *request)
{
    styxreadfree(request);
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
                replydata(request, nil, 0, "fid retired");
            else
                retireread(request);
        }
    }
}


/* EN: Stage the data callback for fid using call and its count. Directory
 * requests and zero-byte operations cannot defer. Return 1 on successful defer.
 * RU: Подготовить колбэк I/O для fid из call и count. Запросы каталогов
 * и нулевой длины не откладываются. Вернуть 1 при принятом отложенном запросе. */
static int callbackdata(Client *client, Fid *fid, Fcall *call, ulong *count, int directory, int operation)
{
    StyxRead stage, *request;
    memset(&stage, 0, sizeof(stage));
    stage.client = client;
    stage.fid = fid;
    stage.limit = call->count;
    stage.offset = call->offset;
    stage.tag = call->tag;
    stage.operation = operation;
    stage.directory = directory;
    client->reading = &stage;
    if (operation == Tread)
    {
        call->ename = client->server->ops->read(fid->qid, client->data, count, call->offset);
    }
    else
    {
        call->ename = client->server->ops->write(fid->qid, call->data, count, call->offset);
    }
    client->reading = nil;
    request = stage.adopted;
    if(request == nil)
    {
        return 0;
    }
    request->waiting = 1;
    if(call->ename == nil && *count == 0)
    {
        return 1;
    }
    if(call->ename == nil)
    {
        call->ename = operation == Tread ? "deferred read returned immediate data" :
            "deferred write returned immediate count";
    }
    retireread(request);
    return 0;
}
