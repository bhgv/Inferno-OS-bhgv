/* Copyright (c) 2026 bhgv
 * EN: Original dispatcher and codec tests for deferred byte I/O and cancellation.
 * RU: Проверки исходного диспетчера и кодека для отложенного I/O и отмены.
 */
#define styxsend capture
#define styxclosesocket closefixture
#include "../styxserver.c"
#undef styxsend
#undef styxclosesocket

static Styxserver server; /* Isolated native socket owner. */
static StyxRead *requests[256]; /* Caller-owned test handles, retained after cancellation. */
static int created; /* Adopted contexts. */
static int released; /* Exactly-once context retirements. */
static int mode; /*0 synchronous,1 deferred,2 immediate error,3 mixed data,4 expected refusal. */
static int nreply; /* Number of response frames in this owner operation. */
static uchar responses[4][MSGMAX]; /* Copied original-codec wire replies. */
static int lengths[4]; /* Exact copied frame lengths. */
static int fidfreed; /* Native fid context cleanup oracle. */


/* EN/RU: Always-active native check; no assert or optimized-away effects.
 * Постоянная native check без assert/исчезающих effects. */
static void
check(int valid, char *why)
{
    if(!valid){
        fprint(2, "deferred read: %s\n", why);
        exit(1);
    }
}


/* EN/RU: Copy each real reply so clunk's read-error ordering can be inspected.
 * Скопировать каждый real reply для проверки read-error перед clunk. */
int
capture(Styxserver *s, int fd, char *bytes, int count, int flags)
{
    USED(s); USED(fd); USED(flags);
    check(nreply < 4 && count <= MSGMAX, "response bounds");
    memmove(responses[nreply], bytes, count);
    lengths[nreply++] = count;
    return count;
}


/* EN/RU: Fixture never owns a socket descriptor.
 * Fixture не владеет socket descriptor. */
void
closefixture(int fd)
{
    USED(fd);
}


/* EN/RU: Context is retired while its request is already detached but fid storage lives.
 * Context завершается после detach request, пока fid storage ещё живёт. */
static void
release(void *opaque)
{
    StyxRead *request = *(StyxRead**)opaque;
    check(styxreadclient(request) == nil && styxreadaux(request) == nil && !styxreadlimit(request), "release before detach");
    released++;
}


/* EN/RU: Fid cleanup follows pending-read cleanup, never leaves borrowed aux dangling.
 * Fid cleanup следует за pending-read cleanup, не оставляет borrowed aux. */
static void
freecontext(void *opaque)
{
    USED(opaque);
    check(released == created, "fid freed before pending read context");
    fidfreed++;
}


/* EN/RU: Existing synchronous callback opts in without retaining transport buffers.
 * Существующий synchronous callback выбирает defer без удержания transport buffers. */
static char*
readfile(Qid qid, char *bytes, ulong *count, vlong offset)
{
    StyxRead *request;
    USED(qid.path); USED(offset);
    if(mode == 0){
        *count = styxreadstr(0, bytes, *count, "sync");
        return nil;
    }
    check(created < 256, "fixture handle capacity");
    request = styxdeferread(&server, &requests[created], release);
    if(mode == 4){
        check(request == nil, "forbidden defer accepted");
        return Eperm;
    }
    check(request != nil, "defer allocation");
    requests[created++] = request;
    check(styxdeferread(&server, nil, nil) == nil, "duplicate defer accepted");
    check(styxreadclient(request) == nil && !styxreadreply(request, "bad", 3, nil), "completion inside callback");
    *count = mode == 3 ? 1 : 0;
    return mode == 2 ? Eperm : nil;
}


/* EN/RU: Feed one real codec request into the actual native dispatcher.
 * Передать real codec request в настоящий native dispatcher. */
static void
sendrequest(Client *client, Fcall *call)
{
    client->nc = 0;
    client->nread = convS2M(call, (uchar*)client->msg, sizeof(client->msg));
    check(client->nread > 0, "request encoding");
    client->state = CNREAD;
    server.curc = client;
    nreply = 0;
    run(client);
}


/* EN/RU: Decode a captured reply with original codec; preserve source bytes for repeated checks.
 * Decode captured reply исходным codec; source bytes не менять. */
static void
response(int index, int type, ushort tag, char *data)
{
    uchar bytes[MSGMAX];
    Fcall reply;
    check(index < nreply, "missing response");
    memmove(bytes, responses[index], lengths[index]);
    check(convM2S(bytes, lengths[index], &reply) == lengths[index] && reply.type == type && reply.tag == tag, "response type/tag");
    if(data != nil)
        check(reply.count == strlen(data) && !memcmp(reply.data, data, reply.count), "deferred payload");
}


/* EN/RU: Construct independently open equal-Qid fids with full32-bit identities.
 * Создать independent open equal-Qid fids с full32-bit identities. */
static Fid*
opened(Client *client, u32int number)
{
    Fid *fid = newfid(client, number, styxqid(0x1234567800000001ULL, 0));
    fid->open = 1;
    fid->mode = OREAD;
    return fid;
}


/* EN/RU: Clear caller refs after transaction retirement, exactly once per context.
 * Убрать caller refs после retirement, по одному на context. */
static void
clearhandles(void)
{
    int i;
    check(created == released, "missing context release");
    for(i = 0; i < created; i++){
        check(!styxreadreply(requests[i], "late", 4, nil), "retired read revived");
        check(!styxwritereply(requests[i], 1, nil), "retired write revived");
        styxreadfree(requests[i]);
        requests[i] = nil;
    }
    created = released = 0;
}


/* EN: Defer one three-byte write after checking its input and original offset.
 * RU: Отложить запись трёх байтов после проверки входа и исходного смещения. */
static char *writefile(Qid qid, char *bytes, ulong *count, vlong offset)
{
    USED(qid.path);
    check(*count == 3 && !memcmp(bytes, "put", 3) && offset == 0x100000001LL, "write arguments");
    check(created < 256, "fixture handle capacity");
    StyxWrite *request = styxdeferwrite(&server, &requests[created], release);
    check(request && !styxdeferread(&server, nil, nil) && !styxwritereply(request, 1, nil),
        "write stage rules");
    requests[created++] = request;
    *count = 0;
    return nil;
}


/* EN: Admit a write and read on separate fids, flush by tag and check exact Rwrite.
 * RU: Принять запись и чтение на разных fid, отменить по тегу и проверить Rwrite. */
static void writes(void)
{
    Client *client = newclient(&server, -1);
    client->versioned = 1;
    opened(client, 1)->mode = ORDWR;
    opened(client, 2);
    Fcall write = {.type = Twrite, .tag = 3, .fid = 1, .offset = 0x100000001LL, .count = 3, .data = "put"};
    Fcall read = {.type = Tread, .tag = 4, .fid = 2, .count = 1};
    mode = 1;
    sendrequest(client, &write);
    check(nreply == 0 && created == 1, "deferred Twrite returned early");
    sendrequest(client, &read);
    check(nreply == 0 && created == 2, "another fid did not proceed");
    Fcall flush = {.type = Tflush, .tag = 5, .oldtag = 3};
    sendrequest(client, &flush);
    response(0, Rflush, 5, nil);
    check(released == 1 && !styxwritereply(requests[0], 3, nil) &&
        styxreadreply(requests[1], "r", 1, nil), "write flush affected the other read");
    response(1, Rread, 4, "r");
    clearhandles();

    sendrequest(client, &write);
    StyxWrite *pending = requests[0];
    check(!styxreadreply(pending, "r", 1, nil) && !styxwritereply(pending, 4, nil) &&
        !styxwritereply(pending, 1, Eperm) && styxwritereply(pending, 2, nil), "write acknowledgement limits");
    response(0, Rwrite, 3, nil);
    check(GBIT32(responses[0] + 7) == 2, "short write count");
    clearhandles();

    sendrequest(client, &write);
    Fcall clunk = {.type = Tclunk, .tag = 6, .fid = 1};
    sendrequest(client, &clunk);
    check(nreply == 2 && released == 1, "clunk did not retire the write");
    response(0, Rerror, 3, nil);
    response(1, Rclunk, 6, nil);
    clearhandles();
    freeclient(client);
    server.curc = nil;
}


/* EN/RU: Exercise success/error, msize/offset, cancellation, quota and disconnected lifetime.
 * Проверить success/error, msize/offset, cancellation, quota и disconnected lifetime. */
int
main(void)
{
    Styxops ops;
    Styxfile *table[TABSZ];
    Client *client;
    Fcall call;
    StyxRead *first, *second;
    int i;
    memset(&ops, 0, sizeof(ops));
    memset(table, 0, sizeof(table));
    ops.read = readfile;
    ops.write = writefile;
    ops.freefid = freecontext;
    server.ops = &ops;
    server.ftab = table;
    client = newclient(&server, -1);
    client->versioned = 1;
    client->msize = 256;
    Fid *context = opened(client, 0x10001);
    context->aux = &server;
    context->auxfree = freecontext;
    opened(client, 0x20001);
    memset(&call, 0, sizeof(call));
    call.type = Tread;
    call.tag = 10;
    call.fid = 0x10001;
    call.offset = 0x1234567800000000LL;
    call.count = 1000;
    sendrequest(client, &call);
    response(0, Rread, 10, "sync");
    mode = 4;
    call.count = 0;
    sendrequest(client, &call);
    response(0, Rerror, 10, nil);
    call.count = 1000;
    call.tag = NOTAG;
    sendrequest(client, &call);
    response(0, Rerror, NOTAG, nil);
    call.tag = 10;
    opened(client, 0x30001)->qid.type = QTDIR;
    call.fid = 0x30001;
    sendrequest(client, &call);
    response(0, Rerror, 10, nil);
    call.fid = 0x10001;
    mode = 1;
    sendrequest(client, &call);
    first = requests[0];
    check(!nreply && styxreadclient(first) == client && styxreadaux(first) == &server &&
          styxreadlimit(first) == 256-IOHDRSZ && styxreadoffset(first) == call.offset, "pending captured scope64");
    check(!styxreadreply(first, nil, 1, nil) && !styxreadreply(first, "x", 1000, nil) &&
          !styxreadreply(first, "x", 1, Eperm) && !nreply, "invalid completion consumed request");
    check(styxreadreply(first, "one", 3, nil) && released == 1, "delayed success");
    response(0, Rread, 10, "one");
    clearhandles();
    sendrequest(client, &call);
    first = requests[0];
    check(styxreadreply(first, nil, 0, Eperm), "deferred denial");
    response(0, Rerror, 10, nil);
    clearhandles();
    sendrequest(client, &call);
    first = requests[0];
    call.fid = 0x20001;
    call.tag = 11;
    sendrequest(client, &call);
    second = requests[1];
    check(!nreply && first != second, "independent fids did not defer");
    call.type = Tflush;
    call.tag = 12;
    call.oldtag = 99;
    sendrequest(client, &call);
    response(0, Rflush, 12, nil);
    check(styxreadclient(first) && styxreadclient(second), "unrelated flush cancelled work");
    call.oldtag = 10;
    sendrequest(client, &call);
    response(0, Rflush, 12, nil);
    check(!styxreadclient(first) && styxreadclient(second), "matching flush scope");
    call.type = Tclunk;
    call.tag = 13;
    call.fid = 0x20001;
    sendrequest(client, &call);
    check(nreply == 2 && !styxreadclient(second), "clunk did not retire pending read");
    response(0, Rerror, 11, nil);
    response(1, Rclunk, 13, nil);
    clearhandles();
    call.type = Tread;
    call.fid = 0x10001;
    call.tag = 10;
    mode = 2;
    sendrequest(client, &call);
    response(0, Rerror, 10, nil);
    clearhandles();
    mode = 3;
    sendrequest(client, &call);
    response(0, Rerror, 10, nil);
    clearhandles();
    mode = 1;
    sendrequest(client, &call);
    mode = 4;
    call.tag = 14;
    sendrequest(client, &call);
    response(0, Rerror, 14, nil);
    call.type = Tversion;
    call.tag = 15;
    call.msize = 128;
    call.version = VERSION9P;
    sendrequest(client, &call);
    response(0, Rerror, 15, nil);
    check(styxreadclient(requests[0]) == client, "invalid version cancelled pending read");
    call.tag = NOTAG;
    call.msize = 1024;
    call.version = VERSION9P;
    sendrequest(client, &call);
    response(0, Rversion, NOTAG, nil);
    check(fidfreed == 1, "fid cleanup ordering");
    clearhandles();
    mode = 1;
    call.type = Tread;
    call.count = 1;
    for(i = 0; i < 128; i++){
        call.fid = i+1;
        call.tag = i+1;
        opened(client, call.fid);
        sendrequest(client, &call);
        check(!nreply, "bounded admission");
    }
    mode = 4;
    call.fid = 129;
    call.tag = 129;
    opened(client, 129);
    sendrequest(client, &call);
    response(0, Rerror, 129, nil);
    call.type = Tauth;
    call.tag = 1;
    sendrequest(client, &call);
    check(!nreply && (client->state & CDISC), "duplicate pending tag not disconnected");
    freeclient(client);
    clearhandles();
    server.curc = nil;
    check(!styxdeferread(&server, nil, nil), "defer outside callback");
    writes();
    print("PASS native deferred byte I/O, full-width scope, flush, clunk, reset and quota\n");
    return 0;
}
