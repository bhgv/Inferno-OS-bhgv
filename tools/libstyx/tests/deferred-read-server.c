/* Copyright (c) 2026 bhgv
 * EN: Real POSIX Styx server with native producer pipe and one fixture record per pending read.
 * RU: Реальный POSIX Styx server с native producer pipe и одной fixture записью на read.
 */
#include <lib9.h>
#include <styx.h>
#include "styxserver.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>


/* EN/RU: Dispatcher-owned pending context, release consumes caller request reference.
 * Dispatcher-owned pending context; release поглощает caller request reference. */
typedef struct Pending Pending;
struct Pending
{
    Pending *next; /* Socket-owner pending chain. */
    StyxRead *request; /* Owned caller reference through library retirement. */
};
static Styxserver server; /* Sole native owner, no guest execution. */
static Pending *head; /* FIFO pending byte reads. */
static Pending **tail = &head; /* Tail link, rebuilt by retirement. */
static int pipefd[2]; /* Producer writes copied commands; owner drains readiness. */
static unsigned queued; /* Bounded fixture records, no public event encoding. */
static unsigned delivered; /* Alternating fixture payload index. */
static unsigned adopted; /* Deferred request lifetime count. */
static unsigned released; /* Exactly-once native context cleanup count. */
static int stopping; /* Owner-only end request. */


/* EN/RU: Fail explicitly without disabling checks in optimized native builds.
 * Явный отказ с проверками во всех native build modes. */
static void
check(int valid, char *why)
{
    if(!valid){
        fprint(2, "deferred read TCP: %s\n", why);
        exit(2);
    }
}


/* EN/RU: Retire context before fid/client cleanup; never execute guest or await producer.
 * Убрать context до fid/client cleanup; без guest execution и ожидания producer. */
static void
retired(void *opaque)
{
    Pending *node = opaque, **link;
    check(styxreadclient(node->request) == nil, "release while active");
    for(link = &head; *link != nil; link = &(*link)->next){
        if(*link == node){
            *link = node->next;
            break;
        }
    }
    for(tail = &head; *tail != nil; tail = &(*tail)->next){
    }
    styxreadfree(node->request);
    free(node);
    released++;
}


/* EN/RU: Fixture allows only one synthetic endpoint, with ordinary root confinement.
 * Fixture разрешает один synthetic endpoint с обычным root confinement. */
static char*
walk(Qid *qid, char *name)
{
    if(qid->path != Qroot)
        return Enonexist;
    if(!strcmp(name, ".."))
        *qid = styxqid(Qroot, 1);
    else if(!strcmp(name, "events"))
        *qid = styxqid(1, 0);
    else
        return Enonexist;
    return nil;
}


/* EN/RU: Open only readonly root/event fixtures; no implicit write authority.
 * Открывать только readonly root/event fixtures, без write authority. */
static char*
openfile(Qid *qid, int mode)
{
    USED(qid->path);
    return mode == OREAD ? nil : Eperm;
}


/* EN/RU: Capture one deferred native read; synchronous callback retains no data buffer.
 * Захватить один deferred native read; synchronous callback не удерживает data buffer. */
static char*
readfile(Qid qid, char *bytes, ulong *count, vlong offset)
{
    Pending *node;
    USED(bytes); USED(offset);
    if(qid.path != 1)
        return Eperm;
    if(!*count)
        return nil;
    node = calloc(1, sizeof(*node));
    if(node == nil)
        return Enomem;
    node->request = styxdeferread(&server, node, retired);
    if(node->request == nil){
        free(node);
        return "read already pending or unavailable";
    }
    *tail = node;
    tail = &node->next;
    adopted++;
    *count = 0;
    print("pending\n");
    return nil;
}


/* EN/RU: Report synthetic attributes without interpreting wire user as trusted identity.
 * Выдать synthetic attributes, не считая wire user доверенной identity. */
static char*
statfile(Qid qid, Dir *out)
{
    memset(out, 0, sizeof(*out));
    out->qid = qid;
    out->mode = qid.path == Qroot ? DMDIR|0555 : 0444;
    out->name = qid.path == Qroot ? "/" : "events";
    out->uid = out->gid = out->muid = "fixture";
    return nil;
}


/* EN/RU: Owner completes one record per read; a short request preserves queued record.
 * Owner выдаёт одну запись на read; short request сохраняет queued record. */
static void
complete(void)
{
    while(head != nil && queued){
        if(styxreadlimit(head->request) < 3)
            check(styxreadreply(head->request, nil, 0, "record exceeds read capacity"), "short read reply");
        else{
            check(styxreadreply(head->request, delivered%2 ? "two" : "one", 3, nil), "record reply");
            delivered++;
            queued--;
        }
    }
}


/* EN/RU: Independent native producer forwards copied external fixture commands through pipe.
 * Независимый native producer передаёт copied fixture commands через pipe. */
static void*
producer(void *unused)
{
    char byte;
    int n;
    USED(unused);
    for(;;){
        do{
            n = read(0, &byte, 1);
        }while(n < 0 && errno == EINTR);
        if(n <= 0)
            byte = 's';
        check(write(pipefd[1], &byte, 1) == 1, "producer signal");
        if(byte == 's')
            return nil;
    }
}


/* EN/RU: Serve actual TCP until producer stop; explicit join only during native server teardown.
 * Обслуживать TCP до producer stop; explicit join только при native teardown. */
int
main(void)
{
    Styxops ops;
    pthread_t worker;
    struct sockaddr_in address;
    socklen_t size = sizeof(address);
    char command;
    memset(&ops, 0, sizeof(ops));
    ops.walk = walk;
    ops.open = openfile;
    ops.read = readfile;
    ops.stat = statfile;
    check(!pipe(pipefd) && !fcntl(pipefd[0], F_SETFL, O_NONBLOCK), "native pipe");
    check(styxinit(&server, &ops, "0", 0555, 0) == nil, "server init");
    check(!styxnotifyfd(&server, pipefd[0]), "register native readiness");
    check(!pthread_create(&worker, nil, producer, nil), "producer thread");
    check(!getsockname(server.connfd, (struct sockaddr*)&address, &size), "port");
    print("%d\n", ntohs(address.sin_port));
    while(!stopping){
        check(styxwait(&server) == nil && styxprocess(&server) == nil, "native event loop");
        while(read(pipefd[0], &command, 1) == 1){
            if(command == 's')
                stopping = 1;
            else if(command == 'e'){
                check(queued < 4, "fixture queue bound");
                queued++;
            }
        }
        if(!stopping)
            complete();
    }
    check(styxend(&server) == nil && !pthread_join(worker, nil), "server teardown");
    close(pipefd[0]);
    close(pipefd[1]);
    check(head == nil && adopted == released, "request context lifetime");
    print("PASS original C deferred read TCP: %ud retired\n", released);
    return 0;
}
