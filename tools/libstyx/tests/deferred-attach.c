/* Copyright (c) 2026 bhgv
 * EN: Original dispatcher/codec oracle for delayed attach and cancellation lifetime.
 * RU: Исходный dispatcher/codec: отложенный attach и lifetime отмены.
 */
#define styxsend capture
#define styxclosesocket closefixture
#include "../styxserver.c"
#undef styxsend
#undef styxclosesocket

static Styxserver server; /* Isolated socket owner, no managed execution. */
static StyxAttach *pending; /* Caller-owned handle, can outlive the client. */
static uchar reply[MSGMAX]; /* Copied real-codec reply. */
static int nreply, released, reject; /* Native oracle counters and admission control. */


/* EN/RU: Fail explicitly in every build configuration.
 * Явный отказ в любой конфигурации сборки. */
static void
check(int ok, char *why)
{
    if(!ok){
        fprint(2, "deferred attach: %s\n", why);
        exit(1);
    }
}


/* EN/RU: Capture the encoded reply without borrowing stack storage.
 * Скопировать кодированный ответ, не заимствовать stack storage. */
int
capture(Styxserver *s, int fd, char *bytes, int count, int flags)
{
    USED(s); USED(fd); USED(flags);
    memmove(reply, bytes, count);
    nreply = count;
    return count;
}


/* EN/RU: The oracle owns no transport descriptor.
 * Oracle не владеет transport descriptor. */
void
closefixture(int fd)
{
    USED(fd);
}


/* EN/RU: A request is inactive before its release callback runs.
 * Request неактивен до вызова release callback. */
static void
release(void *aux)
{
    USED(aux);
    check(styxattachclient(pending) == nil, "cleanup while request still live");
    released++;
}


/* EN/RU: Defer using only the callback's native owner scope.
 * Отложить только из native owner scope callback. */
static char*
admit(char *uname, char *aname)
{
    USED(uname); USED(aname);
    pending = styxdeferattach(&server, nil, release);
    check(pending != nil, "defer ownership");
    check(styxdeferattach(&server, nil, nil) == nil, "duplicate defer");
    check(styxattachclient(pending) == nil && !styxattachreply(pending, nil), "completion inside callback");
    return reject ? Eperm : nil;
}


/* EN/RU: Feed one real wire request and return reply type,0 if truly deferred.
 * Передать настоящий wire request; вернуть reply type либо0 при defer. */
static int
request(Client *client, Fcall *call)
{
    Fcall out;
    client->nc = 0;
    client->nread = convS2M(call, (uchar*)client->msg, sizeof(client->msg));
    check(client->nread > 0, "encode request");
    client->state = CNREAD;
    nreply = 0;
    server.curc = client;
    run(client);
    if(!nreply)
        return 0;
    check(convM2S(reply, nreply, &out) == nreply && out.tag == call->tag, "reply tag/codec");
    return out.type;
}


/* EN/RU: Check actual tags, fid reservation, reset, immediate errors and disconnect.
 * Проверить tags, резервирование fid, reset, immediate error и disconnect. */
int
main(void)
{
    Styxops ops;
    Styxfile *table[TABSZ];
    Client *client;
    StyxAttach *old;
    Fcall call, out;
    memset(&ops, 0, sizeof(ops));
    memset(table, 0, sizeof(table));
    server.ops = &ops;
    server.ftab = table;
    ops.attach = admit;
    client = newclient(&server, -1);
    client->versioned = 1;
    client->msize = 1024;
    newfid(client, 77, styxqid(8, 1));
    memset(&call, 0, sizeof(call));
    call.type = Tattach;
    call.tag = 41;
    call.fid = 1;
    call.afid = NOFID;
    call.uname = "inferno";
    call.aname = "";
    check(request(client, &call) == 0 && findfid(client, 1) == nil, "sent premature reply/fid");
    check(styxattachclient(pending) == client && !strcmp(client->uname, "inferno"), "pending scope");
    old = pending;
    call.tag = 42;
    check(request(client, &call) == Rerror && pending == old, "second attach overwrote pending");
    call.type = Twalk;
    call.fid = 77;
    call.newfid = 1;
    call.nwname = 0;
    check(request(client, &call) == Rerror, "reserved attach fid reused by walk");
    call.type = Tflush;
    call.oldtag = 12;
    check(request(client, &call) == Rflush && styxattachclient(old) == client, "unrelated flush");
    call.oldtag = 41;
    check(request(client, &call) == Rflush && released == 1, "flush cleanup");
    nreply = 0;
    check(!styxattachreply(old, nil) && !nreply && findfid(client, 1) == nil, "stale completion after Rflush");
    styxattachfree(old);
    call.type = Tattach;
    call.fid = 1;
    call.tag = 41;
    check(request(client, &call) == 0, "tag/fid reuse after flush");
    check(styxattachreply(pending, nil) == 1 && findfid(client, 1) != nil && released == 2, "delayed success");
    check(convM2S(reply, nreply, &out) == nreply && out.type == Rattach && out.tag == 41, "delayed Rattach tag");
    check(!styxattachreply(pending, nil), "duplicate completion");
    styxattachfree(pending);
    call.fid = 2;
    call.tag = 43;
    check(request(client, &call) == 0 && styxattachreply(pending, Eperm) == 1 && findfid(client, 2) == nil, "delayed denial");
    styxattachfree(pending);
    check(request(client, &call) == 0, "defer before reset");
    old = pending;
    call.type = Tversion;
    call.tag = NOTAG;
    call.msize = 1024;
    call.version = VERSION9P;
    check(request(client, &call) == Rversion && !styxattachclient(old), "version cancellation");
    styxattachfree(old);
    call.type = Tattach;
    call.tag = 44;
    reject = 1;
    check(request(client, &call) == Rerror && !styxattachclient(pending), "immediate error after defer");
    styxattachfree(pending);
    reject = 0;
    check(request(client, &call) == 0, "defer before disconnect");
    old = pending;
    call.type = Tauth;
    check(request(client, &call) == 0 && (client->state & CDISC), "duplicate pending tag must disconnect");
    freeclient(client);
    check(!styxattachclient(old) && !styxattachreply(old, nil) && released == 6, "disconnect stale handle");
    styxattachfree(old);
    server.curc = nil;
    check(styxdeferattach(&server, nil, nil) == nil, "defer outside callback");
    print("PASS native deferred attach, tags, flush, reset and disconnected lifetime\n");
    return 0;
}
