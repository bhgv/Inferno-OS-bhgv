#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#define Egreg "inconsistent state" // Native generic connection failure.
#define nil NULL // Inferno null pointer spelling.
#define ORDWR 2 // Native read/write open mode.
#define ERRMAX 128 // Error text capacity.
#define NETPATHLEN 40 // Public network directory capacity.
#define USED(x) ((void)(x)) // Mark an intentionally unused argument.



/* This allocation-free fixture needs no LSan /proc process inspection. */
int
__lsan_is_turned_off(void)
{
	return 1;
}

static char errorbuf[ERRMAX];
static char reply[256];
static char opened[256];
static int opens, closes, failopen, rejectopen;
static long reported, secondcount;
static int reads, tworeads;
static unsigned long cases;
static int writecalls; // Number of native command writes observed.
static int failwrite; // One-based write call with an injected reply; zero disables it.
static long writeresult; // Exact injected native result, including oversized values.


/* Keep predicates active when the production source is compiled with NDEBUG. */
static void
check(int ok, const char *reason)
{
	if(!ok){
		fprintf(stderr, "dial predicate: %s\n", reason);
		exit(1);
	}
	cases++;
}


/* Format using the bounded native helper's stored-byte return convention. */
static int
snprint(char *buf, int size, const char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(buf, size, fmt, args);
	va_end(args);
	return n >= size ? size-1 : n;
}


/* Retain the legacy unbounded formatter to expose real source overflows. */
static int
sprint(char *buf, const char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vsprintf(buf, fmt, args);
	va_end(args);
	return n;
}


/* Record error text without introducing VM or scheduler dependencies. */
static void
kwerrstr(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(errorbuf, sizeof(errorbuf), fmt, args);
	va_end(args);
}


/* Swap error strings as the native kernel interface does. */
static void
kerrstr(char *buf, int size)
{
	char saved[ERRMAX];
	int n = size < ERRMAX ? size : ERRMAX;

	memmove(saved, buf, n);
	memmove(buf, errorbuf, n);
	memmove(errorbuf, saved, n);
	if(n > 0){
		buf[n-1] = 0;
		errorbuf[n-1] = 0;
	}
}


/* Track resource ownership and the exact selected native path. */
static int
kopen(char *path, int mode)
{
	USED(mode);
	check(!rejectopen, "invalid directory must not open native path");
	opens++;
	snprint(opened, sizeof(opened), "%s", path);
	return failopen ? -1 : 20;
}


/* Count descriptor releases on rejected replies. */
static int
kclose(int fd)
{
	USED(fd);
	closes++;
	return 0;
}


/* Copy only the requested bytes while independently reporting a native count. */
static long
kread(int fd, void *buf, long size)
{
	size_t n = strlen(reply);

	USED(fd);
	if(tworeads && reads++ > 0){
		reported = secondcount;
		strcpy(reply, "1");
		n = 1;
	}
	check(size >= 0, "nonnegative read capacity");
	if(n > (size_t)size)
		n = size;
	memmove(buf, reply, n);
	return reported;
}


/* Successful request writes leave path parsing as the operation under test. */
static long
kwrite(int fd, void *buf, long size)
{
	USED(fd);
	USED(buf);
	writecalls++;
	if(writecalls == failwrite){
		if(writeresult < 0)
			kwerrstr("native write failed");
		return writeresult;
	}
	return size;
}


/* Connection-server rewinds have no external side effect in this fixture. */
static long
kseek(int fd, long offset, int origin)
{
	USED(fd);
	USED(origin);
	return offset;
}


/* PRODUCTION_SOURCE */


/* Reset all native observations between independent cases. */
static void
reset(const char *text, long count)
{
	opens = closes = failopen = reads = tworeads = rejectopen = 0;
	writecalls = failwrite = 0;
	writeresult = 0;
	reported = count;
	snprint(reply, sizeof(reply), "%s", text);
	opened[0] = errorbuf[0] = 0;
}


/* Check parse handling through the real native implementation. */
static void
testparse(void)
{
	DS ds;
	char input[300];
	int i;

	for(i = 1; i < 128; i++){
		memset(input, 'x', i);
		input[0] = '#';
		input[i] = '!';
		input[i+1] = 0;
		reset("", 0);
		check(_dial_string_parse(input, &ds) < 0, "missing root slash rejected");
		check(opens == 0, "bad parse has no native side effect");
	}
	for(i = 128; i < sizeof(input)-1; i++){
		memset(input, 'x', i);
		input[i] = 0;
		check(_dial_string_parse(input, &ds) < 0, "long destination rejected");
	}
	check(_dial_string_parse("tcp!host!80", &ds) == 0 && ds.netdir == nil &&
	      strcmp(ds.proto, "tcp") == 0 && strcmp(ds.rem, "host!80") == 0,
	      "unrooted protocol remains compatible");
	check(_dial_string_parse("#I/tcp!host", &ds) == 0 &&
	      strcmp(ds.netdir, "#I") == 0 && strcmp(ds.proto, "tcp") == 0,
	      "device-rooted protocol remains compatible");
	check(_dial_string_parse("host", &ds) == 0 && strcmp(ds.proto, "net") == 0,
	      "implicit net protocol remains compatible");
}


/* Check translate handling through the real native implementation. */
static void
testtranslate(void)
{
	char file[128], address[128];
	int i;

	for(i = 0; i < 3; i++){
		const char *paths[] = {"tcp/clone destination", "/net/tcp/clone destination",
		                      "/net.alt/tcp/clone destination"};
		reset(paths[i], strlen(paths[i]));
		check(nettrans("tcp!host", address, sizeof(address), file, sizeof(file)) == 0,
		      "translation accepted");
		check(strcmp(file, "/net/tcp/clone") == 0, "translated clone path");
		check(strcmp(address, "destination") == 0 && closes == 1,
		      "destination and descriptor preserved");
	}
}


/* Check count handling through the real native implementation. */
static void
testcount(void)
{
	DS ds;
	char file[128], address[128], directory[NETPATHLEN];
	long counts[] = {100, 128, INT_MAX, (long)INT_MAX+1, LONG_MAX};
	int i;

	for(i = 0; i < sizeof(counts)/sizeof(counts[0]); i++){
		reset("tcp/clone destination", counts[i]);
		if(counts[i] >= 100){
			check(nettrans("tcp!host", address, sizeof(address), file, sizeof(file)) < 0,
			      "wide translation count rejected");
			check(closes == 1, "translation fd closed");
		}
		reset("1", counts[i]);
		memset(&ds, 0, sizeof(ds));
		strcpy(file, "/net/tcp/clone");
		check(call(file, "host", &ds) < 0 && closes == 1,
		      "wide clone count rejected and closed");
		reset("tcp/clone destination", counts[i]);
		ds.netdir = "/net";
		ds.proto = "tcp";
		ds.rem = "host";
		if(counts[i] >= 128)
			check(csdial(&ds) < 0 && closes == 1, "wide cs iteration rejected");
		reset("1", counts[i]);
		check(klisten("/net/tcp/0", directory) < 0 && closes == 1,
		      "wide listen count rejected");
	}
}


/* Check listen handling through the real native implementation. */
static void
testlisten(void)
{
	char input[300], directory[NETPATHLEN];
	int i, j, rv;

	for(i = 0; i < sizeof(input)-1; i++){
		memset(input, 'x', i);
		input[i] = 0;
		reset("1", 1);
		rejectopen = 1;
		check(klisten(input, directory) < 0 && opens == 0,
		      "slashless listen rejected before open");
		if(i >= NETPATHLEN-7){
			input[0] = '/';
			rejectopen = 1;
			check(klisten(input, directory) < 0 && opens == 0,
			      "long listen rejected before open");
		}
	}
	rejectopen = 0;
	for(i = 1; i <= NETPATHLEN-8; i++){
		memset(input, 'x', i);
		input[0] = '/';
		input[i] = 0;
		for(j = 1; j < NETPATHLEN-1; j++){
			memset(reply, '1', j);
			reply[j] = 0;
			reported = j;
			opens = closes = 0;
			rv = klisten(input, directory);
			check(rv == 20 && strlen(directory) == j+1,
			      "bounded listen reply accepted");
			check(opens == 1 && closes == 0, "successful listen transfers ownership");
		}
	}
	for(i = 1; i <= NETPATHLEN-8; i++){
		memset(input, 'x', i);
		input[i-1] = '/';
		input[i] = 0;
		for(j = 1; j <= NETPATHLEN; j++){
			reset("1", j);
			memset(reply, '1', j);
			reply[j] = 0;
			memset(directory, 0x5a, sizeof(directory));
			rv = klisten(input, directory);
			if(j < NETPATHLEN-i)
				check(rv == 20 && closes == 0 && strlen(directory) == i+j,
				      "listen prefix and read fit together");
			else{
				check(rv < 0 && closes == 1, "listen prefix leaves insufficient space");
				check(directory[0] == 0x5a && directory[NETPATHLEN-1] == 0x5a,
				      "failed listen preserves destination");
			}
		}
	}
}


/* Check announce handling through the real native implementation. */
static void
testannounce(void)
{
	char directory[NETPATHLEN];
	int i, j, rv;

	for(i = 1; i < 160; i++){
		reset("/net/tcp/clone host", strlen("/net/tcp/clone host"));
		tworeads = 1;
		secondcount = i;
		memset(directory, 0x5a, sizeof(directory));
		rv = kannounce("tcp!host", directory);
		if(i < NETPATHLEN-9){
			check(rv == 20 && closes == 1, "announce transfers control ownership");
			check(strncmp(directory, "/net/tcp/", 9) == 0, "announce translated directory");
		}else{
			check(rv < 0 && closes == 2, "announce bad count closes control");
			for(j = 0; j < sizeof(directory); j++)
				check(directory[j] == 0x5a, "announce failure preserves result");
		}
	}
}


/* Check clone handling through the real native implementation. */
static void
testclone(void)
{
	DS ds;
	char file[128];

	reset("1", 1);
	memset(&ds, 0, sizeof(ds));
	strcpy(file, "clone");
	check(call(file, "host", &ds) < 0 && closes == 1, "slashless clone rejected and closed");
}


/* Reject partial/oversized native writes without opening data or publishing cfd.
 * Each operation owns its handles; adapters only count native boundaries. */
static void
testwrite(void)
{
	long results[] = {-1, 0, 1, LONG_MAX};
	unsigned int index;
	int operation;

	for(operation = 0; operation < 4; operation++){
		for(index = 0; index < sizeof(results)/sizeof(results[0]); index++){
			DS ds;
			char file[128], address[128], directory[NETPATHLEN];
			int control = -77;
			int result;

			reset(operation == 1 ? "1" : "/net/tcp/clone host",
			      operation == 1 ? 1 : strlen("/net/tcp/clone host"));
			memset(&ds, 0, sizeof(ds));
			ds.netdir = "/net";
			ds.proto = "tcp";
			ds.rem = "host";
			ds.cfdp = &control;
			strcpy(file, "/net/tcp/clone");
			failwrite = operation == 3 ? 2 : 1;
			writeresult = results[index];
			if(operation == 0)
				result = csdial(&ds);
			else if(operation == 1)
				result = call(file, "host", &ds);
			else if(operation == 2)
				result = nettrans("tcp!host", address, sizeof(address), file, sizeof(file));
			else{
				tworeads = 1;
				secondcount = 1;
				result = kannounce("tcp!host", directory);
			}
			check(result < 0, "incomplete command must not succeed");
			check(control == -77, "failed command must not publish control ownership");
			check(opens == (operation == 3 ? 2 : 1) && closes == opens,
			      "failed command closes every opened handle and never opens data");
			check(strstr(errorbuf, results[index] < 0 ? "native write" : "incomplete dial command") != nil,
			      "native error or explicit incomplete-write diagnostic preserved");
		}
	}
}


/* Run all families or one independently reproducible regression control. */
int
main(int argc, char **argv)
{
	if(argc == 1 || strcmp(argv[1], "parse") == 0)
		testparse();
	if(argc == 1 || strcmp(argv[1], "translate") == 0)
		testtranslate();
	if(argc == 1 || strcmp(argv[1], "count") == 0)
		testcount();
	if(argc == 1 || strcmp(argv[1], "listen") == 0)
		testlisten();
	if(argc == 1 || strcmp(argv[1], "announce") == 0)
		testannounce();
	if(argc == 1 || strcmp(argv[1], "clone") == 0)
		testclone();
	if(argc == 1 || strcmp(argv[1], "write") == 0)
		testwrite();
	printf("PASS dial paths: %lu predicates\n", cases);
	return 0;
}
